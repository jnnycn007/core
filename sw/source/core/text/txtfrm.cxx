/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following license notice:
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements. See the NOTICE file distributed
 *   with this work for additional information regarding copyright
 *   ownership. The ASF licenses this file to you under the Apache
 *   License, Version 2.0 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */

#include <config_wasm_strip.h>

#include <hintids.hxx>
#include <hints.hxx>
#include <svl/ctloptions.hxx>
#include <editeng/lspcitem.hxx>
#include <editeng/lrspitem.hxx>
#include <editeng/brushitem.hxx>
#include <editeng/charhiddenitem.hxx>
#include <editeng/pgrditem.hxx>
#include <comphelper/configuration.hxx>
#include <swmodule.hxx>
#include <SwSmartTagMgr.hxx>
#include <doc.hxx>
#include <IDocumentSettingAccess.hxx>
#include <IDocumentDeviceAccess.hxx>
#include <IDocumentFieldsAccess.hxx>
#include <rootfrm.hxx>
#include <pagefrm.hxx>
#include <viewsh.hxx>
#include <pam.hxx>
#include <ndtxt.hxx>
#include <paratr.hxx>
#include <viewopt.hxx>
#include <flyfrm.hxx>
#include <tabfrm.hxx>
#include <frmatr.hxx>
#include <frmtool.hxx>
#include <tgrditem.hxx>
#include <dbg_lay.hxx>
#include <fmtfld.hxx>
#include <fmtftn.hxx>
#include <txtfld.hxx>
#include <txtftn.hxx>
#include <ftninfo.hxx>
#include <fmtline.hxx>
#include <txtfrm.hxx>
#include <notxtfrm.hxx>
#include <sectfrm.hxx>
#include "itrform2.hxx"
#include "widorp.hxx"
#include "txtcache.hxx"
#include <fntcache.hxx>
#include <SwGrammarMarkUp.hxx>
#include <lineinfo.hxx>
#include <SwPortionHandler.hxx>
#include <dcontact.hxx>
#include <sortedobjs.hxx>
#include <txtflcnt.hxx>
#include <fmtflcnt.hxx>
#include <fmtcntnt.hxx>
#include <numrule.hxx>
#include <GrammarContact.hxx>
#include <calbck.hxx>
#include <ftnidx.hxx>
#include <ftnfrm.hxx>

#include <wrtsh.hxx>
#include <view.hxx>
#include <edtwin.hxx>
#include <FrameControlsManager.hxx>

namespace sw {

    MergedAttrIterBase::MergedAttrIterBase(SwTextFrame const& rFrame)
        : m_pMerged(rFrame.GetMergedPara())
        , m_pNode(m_pMerged ? nullptr : rFrame.GetTextNodeFirst())
        , m_CurrentExtent(0)
        , m_CurrentHint(0)
    {
    }

    SwTextAttr const* MergedAttrIter::NextAttr(SwTextNode const** ppNode)
    {
        if (m_pMerged)
        {
            while (m_CurrentExtent < m_pMerged->extents.size())
            {
                sw::Extent const& rExtent(m_pMerged->extents[m_CurrentExtent]);
                if (SwpHints const*const pHints = rExtent.pNode->GetpSwpHints())
                {
                    while (m_CurrentHint < pHints->Count())
                    {
                        SwTextAttr *const pHint(pHints->Get(m_CurrentHint));
                        if (rExtent.nEnd < pHint->GetStart()
                                // <= if it has no end or isn't empty
                            || (rExtent.nEnd == pHint->GetStart()
                                && (!pHint->GetEnd()
                                    || *pHint->GetEnd() != pHint->GetStart())))
                        {
                            break;
                        }
                        ++m_CurrentHint;
                        if (rExtent.nStart <= pHint->GetStart())
                        {
                            if (ppNode)
                            {
                                *ppNode = rExtent.pNode;
                            }
                            return pHint;
                        }
                    }
                }
                ++m_CurrentExtent;
                if (m_CurrentExtent < m_pMerged->extents.size() &&
                    rExtent.pNode != m_pMerged->extents[m_CurrentExtent].pNode)
                {
                    m_CurrentHint = 0; // reset
                }
            }
            return nullptr;
        }
        else
        {
            SwpHints const*const pHints(m_pNode->GetpSwpHints());
            if (pHints)
            {
                if (m_CurrentHint < pHints->Count())
                {
                    SwTextAttr const*const pHint(pHints->Get(m_CurrentHint));
                    ++m_CurrentHint;
                    if (ppNode)
                    {
                        *ppNode = m_pNode;
                    }
                    return pHint;
                }
            }
            return nullptr;
        }
    }

    MergedAttrIterByEnd::MergedAttrIterByEnd(SwTextFrame const& rFrame)
        : m_pNode(rFrame.GetMergedPara() ? nullptr : rFrame.GetTextNodeFirst())
        , m_CurrentHint(0)
    {
        if (!m_pNode)
        {
            MergedAttrIterReverse iter(rFrame);
            SwTextNode const* pNode(nullptr);
            while (SwTextAttr const* pHint = iter.PrevAttr(&pNode))
            {
                m_Hints.emplace_back(pNode, pHint);
            }
        }
    }

    SwTextAttr const* MergedAttrIterByEnd::NextAttr(SwTextNode const*& rpNode)
    {
        if (m_pNode)
        {
            SwpHints const*const pHints(m_pNode->GetpSwpHints());
            if (pHints)
            {
                if (m_CurrentHint < pHints->Count())
                {
                    SwTextAttr const*const pHint(
                            pHints->GetSortedByEnd(m_CurrentHint));
                    ++m_CurrentHint;
                    rpNode = m_pNode;
                    return pHint;
                }
            }
            return nullptr;
        }
        else
        {
            if (m_CurrentHint < m_Hints.size())
            {
                auto const ret = m_Hints[m_Hints.size() - m_CurrentHint - 1];
                ++m_CurrentHint;
                rpNode = ret.first;
                return ret.second;
            }
            return nullptr;
        }
    }

    void MergedAttrIterByEnd::PrevAttr()
    {
        assert(0 < m_CurrentHint); // should only rewind as far as 0
        --m_CurrentHint;
    }

    MergedAttrIterReverse::MergedAttrIterReverse(SwTextFrame const& rFrame)
        : MergedAttrIterBase(rFrame)
    {
        if (m_pMerged)
        {
            m_CurrentExtent = m_pMerged->extents.size();
            SwpHints const*const pHints(0 < m_CurrentExtent
                ? m_pMerged->extents[m_CurrentExtent-1].pNode->GetpSwpHints()
                : nullptr);
            if (pHints)
            {
                pHints->SortIfNeedBe();
                m_CurrentHint = pHints->Count();
            }
        }
        else
        {
            if (SwpHints const*const pHints = m_pNode->GetpSwpHints())
            {
                pHints->SortIfNeedBe();
                m_CurrentHint = pHints->Count();
            }
        }
    }

    SwTextAttr const* MergedAttrIterReverse::PrevAttr(SwTextNode const** ppNode)
    {
        if (m_pMerged)
        {
            while (0 < m_CurrentExtent)
            {
                sw::Extent const& rExtent(m_pMerged->extents[m_CurrentExtent-1]);
                if (SwpHints const*const pHints = rExtent.pNode->GetpSwpHints())
                {
                    while (0 < m_CurrentHint)
                    {
                        SwTextAttr *const pHint(
                                pHints->GetSortedByEnd(m_CurrentHint - 1));
                        if (pHint->GetAnyEnd() < rExtent.nStart
                                // <= if it has end and isn't empty
                            || (pHint->GetEnd()
                                && *pHint->GetEnd() != pHint->GetStart()
                                && *pHint->GetEnd() == rExtent.nStart))
                        {
                            break;
                        }
                        --m_CurrentHint;
                        if (pHint->GetAnyEnd() <= rExtent.nEnd)
                        {
                            if (ppNode)
                            {
                                *ppNode = rExtent.pNode;
                            }
                            return pHint;
                        }
                    }
                }
                --m_CurrentExtent;
                if (0 < m_CurrentExtent &&
                    rExtent.pNode != m_pMerged->extents[m_CurrentExtent-1].pNode)
                {
                    SwpHints const*const pHints(
                        m_pMerged->extents[m_CurrentExtent-1].pNode->GetpSwpHints());
                    m_CurrentHint = pHints ? pHints->Count() : 0; // reset
                    if (pHints)
                        pHints->SortIfNeedBe();
                }
            }
            return nullptr;
        }
        else
        {
            SwpHints const*const pHints(m_pNode->GetpSwpHints());
            if (pHints && 0 < m_CurrentHint)
            {
                SwTextAttr const*const pHint(pHints->GetSortedByEnd(m_CurrentHint - 1));
                --m_CurrentHint;
                if (ppNode)
                {
                    *ppNode = m_pNode;
                }
                return pHint;
            }
            return nullptr;
        }
    }

    bool FrameContainsNode(SwContentFrame const& rFrame, SwNodeOffset const nNodeIndex)
    {
        if (rFrame.IsTextFrame())
        {
            SwTextFrame const& rTextFrame(static_cast<SwTextFrame const&>(rFrame));
            if (sw::MergedPara const*const pMerged = rTextFrame.GetMergedPara())
            {
                SwNodeOffset const nFirst(pMerged->pFirstNode->GetIndex());
                SwNodeOffset const nLast(pMerged->pLastNode->GetIndex());
                return (nFirst <= nNodeIndex && nNodeIndex <= nLast);
            }
            else
            {
                return rTextFrame.GetTextNodeFirst()->GetIndex() == nNodeIndex;
            }
        }
        else
        {
            assert(rFrame.IsNoTextFrame());
            return static_cast<SwNoTextFrame const&>(rFrame).GetNode()->GetIndex() == nNodeIndex;
        }
    }

    bool IsParaPropsNode(SwRootFrame const& rLayout, SwTextNode const& rNode)
    {
        if (rLayout.HasMergedParas())
        {
            if (SwTextFrame const*const pFrame = static_cast<SwTextFrame*>(rNode.getLayoutFrame(&rLayout)))
            {
                sw::MergedPara const*const pMerged(pFrame->GetMergedPara());
                if (pMerged && pMerged->pParaPropsNode != &rNode)
                {
                    return false;
                }
            }
        }
        return true;
    }

    SwTextNode *
    GetParaPropsNode(SwRootFrame const& rLayout, SwNode const& rPos)
    {
        const SwTextNode *const pTextNode(rPos.GetTextNode());
        if (pTextNode && !sw::IsParaPropsNode(rLayout, *pTextNode))
        {
            return static_cast<SwTextFrame*>(pTextNode->getLayoutFrame(&rLayout))->GetMergedPara()->pParaPropsNode;
        }
        else
        {
            return const_cast<SwTextNode*>(pTextNode);
        }
    }

    SwPosition
    GetParaPropsPos(SwRootFrame const& rLayout, SwPosition const& rPos)
    {
        SwPosition pos(rPos);
        SwTextNode const*const pNode(pos.GetNode().GetTextNode());
        if (pNode)
            pos.Assign( *sw::GetParaPropsNode(rLayout, *pNode) );
        return pos;
    }

    std::pair<SwTextNode *, SwTextNode *>
    GetFirstAndLastNode(SwRootFrame const& rLayout, SwNode const& rPos)
    {
        SwTextNode *const pTextNode(const_cast<SwTextNode*>(rPos.GetTextNode()));
        if (pTextNode && rLayout.HasMergedParas())
        {
            if (SwTextFrame const*const pFrame = static_cast<SwTextFrame*>(pTextNode->getLayoutFrame(&rLayout)))
            {
                if (sw::MergedPara const*const pMerged = pFrame->GetMergedPara())
                {
                    return std::make_pair(pMerged->pFirstNode, const_cast<SwTextNode*>(pMerged->pLastNode));
                }
            }
        }
        return std::make_pair(pTextNode, pTextNode);
    }

    SwTextNode const& GetAttrMerged(SfxItemSet & rFormatSet,
            SwTextNode const& rNode, SwRootFrame const*const pLayout)
    {
        rNode.SwContentNode::GetAttr(rFormatSet);
        if (pLayout && pLayout->HasMergedParas())
        {
            auto pFrame = static_cast<SwTextFrame*>(rNode.getLayoutFrame(pLayout));
            if (sw::MergedPara const*const pMerged = pFrame ? pFrame->GetMergedPara() : nullptr)
            {
                if (pMerged->pFirstNode != &rNode)
                {
                    rFormatSet.ClearItem(RES_PAGEDESC);
                    rFormatSet.ClearItem(RES_BREAK);
                    static_assert(RES_PAGEDESC + 1 == sal_uInt16(RES_BREAK),
                            "first-node items must be adjacent");
                    SfxItemSetFixed<RES_PAGEDESC, RES_BREAK> firstSet(*rFormatSet.GetPool());
                    pMerged->pFirstNode->SwContentNode::GetAttr(firstSet);
                    rFormatSet.Put(firstSet);

                }
                if (pMerged->pParaPropsNode != &rNode)
                {
                    for (sal_uInt16 i = RES_PARATR_BEGIN; i != RES_FRMATR_END; ++i)
                    {
                        if (i != RES_PAGEDESC && i != RES_BREAK)
                        {
                            rFormatSet.ClearItem(i);
                        }
                    }
                    for (sal_uInt16 i = XATTR_FILL_FIRST; i <= XATTR_FILL_LAST; ++i)
                    {
                        rFormatSet.ClearItem(i);
                    }
                    SfxItemSetFixed<RES_PARATR_BEGIN, RES_PAGEDESC,
                                   RES_BREAK+1, RES_FRMATR_END,
                                   XATTR_FILL_FIRST, XATTR_FILL_LAST+1>
                         propsSet(*rFormatSet.GetPool());
                    pMerged->pParaPropsNode->SwContentNode::GetAttr(propsSet);
                    rFormatSet.Put(propsSet);
                    return *pMerged->pParaPropsNode;
                }
                // keep all the CHRATR/UNKNOWNATR anyway...
            }
        }
        return rNode;
    }

} // namespace sw

/// Switches width and height of the text frame
void SwTextFrame::SwapWidthAndHeight()
{
    {
        SwFrameAreaDefinition::FramePrintAreaWriteAccess aPrt(*this);

        if ( ! mbIsSwapped )
        {
            const tools::Long nPrtOfstX = aPrt.Pos().X();
            aPrt.Pos().setX( aPrt.Pos().Y() );

            if( IsVertLR() )
            {
                aPrt.Pos().setY( nPrtOfstX );
            }
            else
            {
                aPrt.Pos().setY( getFrameArea().Width() - ( nPrtOfstX + aPrt.Width() ) );
            }
        }
        else
        {
            const tools::Long nPrtOfstY = aPrt.Pos().Y();
            aPrt.Pos().setY( aPrt.Pos().X() );

            if( IsVertLR() )
            {
                aPrt.Pos().setX( nPrtOfstY );
            }
            else
            {
                aPrt.Pos().setX( getFrameArea().Height() - ( nPrtOfstY + aPrt.Height() ) );
            }
        }

        const tools::Long nPrtWidth = aPrt.Width();
        aPrt.Width( aPrt.Height() );
        aPrt.Height( nPrtWidth );
    }

    {
        const tools::Long nFrameWidth = getFrameArea().Width();
        SwFrameAreaDefinition::FrameAreaWriteAccess aFrm(*this);
        aFrm.Width( aFrm.Height() );
        aFrm.Height( nFrameWidth );
    }

    mbIsSwapped = ! mbIsSwapped;
}

/**
 * Calculates the coordinates of a rectangle when switching from
 * horizontal to vertical layout.
 */
void SwTextFrame::SwitchHorizontalToVertical( SwRect& rRect ) const
{
    // calc offset inside frame
    tools::Long nOfstX, nOfstY;
    if ( IsVertLR() )
    {
        if (IsVertLRBT())
        {
            // X and Y offsets here mean the position of the point that will be the top left corner
            // after the switch.
            nOfstX = rRect.Left() + rRect.Width() - getFrameArea().Left();
            nOfstY = rRect.Top() - getFrameArea().Top();
        }
        else
        {
            nOfstX = rRect.Left() - getFrameArea().Left();
            nOfstY = rRect.Top() - getFrameArea().Top();
        }
    }
    else
    {
        nOfstX = rRect.Left() - getFrameArea().Left();
        nOfstY = rRect.Top() + rRect.Height() - getFrameArea().Top();
    }

    const tools::Long nWidth = rRect.Width();
    const tools::Long nHeight = rRect.Height();

    if ( IsVertLR() )
    {
        rRect.Left(getFrameArea().Left() + nOfstY);
    }
    else
    {
        if ( mbIsSwapped )
            rRect.Left( getFrameArea().Left() + getFrameArea().Height() - nOfstY );
        else
            // frame is rotated
            rRect.Left( getFrameArea().Left() + getFrameArea().Width() - nOfstY );
    }

    if (IsVertLRBT())
    {
        if (mbIsSwapped)
            rRect.Top(getFrameArea().Top() + getFrameArea().Width() - nOfstX);
        else
            rRect.Top(getFrameArea().Top() + getFrameArea().Height() - nOfstX);
    }
    else
        rRect.Top(getFrameArea().Top() + nOfstX);
    rRect.Width( nHeight );
    rRect.Height( nWidth );
}

/**
 * Calculates the coordinates of a point when switching from
 * horizontal to vertical layout.
 */
void SwTextFrame::SwitchHorizontalToVertical( Point& rPoint ) const
{
    if (IsVertLRBT())
    {
        // The horizontal origo is the top left corner, the LRBT origo is the
        // bottom left corner. Finally x and y has to be swapped.
        SAL_WARN_IF(!mbIsSwapped, "sw.core",
                    "SwTextFrame::SwitchHorizontalToVertical, IsVertLRBT, not swapped");
        Point aPoint(rPoint);
        rPoint.setX(getFrameArea().Left() + (aPoint.Y() - getFrameArea().Top()));
        // This would be bottom - x delta, but bottom is top + height, finally
        // width (and not height), as it's swapped.
        rPoint.setY(getFrameArea().Top() + getFrameArea().Width()
                    - (aPoint.X() - getFrameArea().Left()));
        return;
    }

    // calc offset inside frame
    const tools::Long nOfstX = rPoint.X() - getFrameArea().Left();
    const tools::Long nOfstY = rPoint.Y() - getFrameArea().Top();
    if ( IsVertLR() )
        rPoint.setX( getFrameArea().Left() + nOfstY );
    else
    {
        if ( mbIsSwapped )
            rPoint.setX( getFrameArea().Left() + getFrameArea().Height() - nOfstY );
        else
            // calc rotated coords
            rPoint.setX( getFrameArea().Left() + getFrameArea().Width() - nOfstY );
    }

    rPoint.setY( getFrameArea().Top() + nOfstX );
}

/**
 * Calculates the a limit value when switching from
 * horizontal to vertical layout.
 */
tools::Long SwTextFrame::SwitchHorizontalToVertical( tools::Long nLimit ) const
{
    Point aTmp( 0, nLimit );
    SwitchHorizontalToVertical( aTmp );
    return aTmp.X();
}

/**
 * Calculates the coordinates of a rectangle when switching from
 * vertical to horizontal layout.
 */
void SwTextFrame::SwitchVerticalToHorizontal( SwRect& rRect ) const
{
    tools::Long nOfstX;

    // calc offset inside frame
    if ( IsVertLR() )
        nOfstX = rRect.Left() - getFrameArea().Left();
    else
    {
        if ( mbIsSwapped )
            nOfstX = getFrameArea().Left() + getFrameArea().Height() - ( rRect.Left() + rRect.Width() );
        else
            nOfstX = getFrameArea().Left() + getFrameArea().Width() - ( rRect.Left() + rRect.Width() );
    }

    tools::Long nOfstY;
    if (IsVertLRBT())
    {
        // Note that mbIsSwapped only affects the frame area, not rRect, so rRect.Height() is used
        // here unconditionally.
        if (mbIsSwapped)
            nOfstY = getFrameArea().Top() + getFrameArea().Width() - (rRect.Top() + rRect.Height());
        else
            nOfstY = getFrameArea().Top() + getFrameArea().Height() - (rRect.Top() + rRect.Height());
    }
    else
        nOfstY = rRect.Top() - getFrameArea().Top();
    const tools::Long nWidth = rRect.Height();
    const tools::Long nHeight = rRect.Width();

    // calc rotated coords
    rRect.Left( getFrameArea().Left() + nOfstY );
    rRect.Top( getFrameArea().Top() + nOfstX );
    rRect.Width( nWidth );
    rRect.Height( nHeight );
}

/**
 * Calculates the coordinates of a point when switching from
 * vertical to horizontal layout.
 */
void SwTextFrame::SwitchVerticalToHorizontal( Point& rPoint ) const
{
    tools::Long nOfstX;

    // calc offset inside frame
    if ( IsVertLR() )
        // X offset is Y - left.
        nOfstX = rPoint.X() - getFrameArea().Left();
    else
    {
        // X offset is right - X.
        if ( mbIsSwapped )
            nOfstX = getFrameArea().Left() + getFrameArea().Height() - rPoint.X();
        else
            nOfstX = getFrameArea().Left() + getFrameArea().Width() - rPoint.X();
    }

    tools::Long nOfstY;
    if (IsVertLRBT())
    {
        // Y offset is bottom - Y.
        if (mbIsSwapped)
            nOfstY = getFrameArea().Top() + getFrameArea().Width() - rPoint.Y();
        else
            nOfstY = getFrameArea().Top() + getFrameArea().Height() - rPoint.Y();
    }
    else
        // Y offset is Y - top.
        nOfstY = rPoint.Y() - getFrameArea().Top();

    // calc rotated coords
    rPoint.setX( getFrameArea().Left() + nOfstY );
    rPoint.setY( getFrameArea().Top() + nOfstX );
}

/**
 * Calculates the a limit value when switching from
 * vertical to horizontal layout.
 */
tools::Long SwTextFrame::SwitchVerticalToHorizontal( tools::Long nLimit ) const
{
    Point aTmp( nLimit, 0 );
    SwitchVerticalToHorizontal( aTmp );
    return aTmp.Y();
}

SwFrameSwapper::SwFrameSwapper( const SwTextFrame* pTextFrame, bool bSwapIfNotSwapped )
    : pFrame( pTextFrame ), bUndo( false )
{
    if (pFrame->IsVertical() && bSwapIfNotSwapped != pFrame->IsSwapped())
    {
        bUndo = true;
        const_cast<SwTextFrame*>(pFrame)->SwapWidthAndHeight();
    }
}

SwFrameSwapper::~SwFrameSwapper()
{
    if ( bUndo )
        const_cast<SwTextFrame*>(pFrame)->SwapWidthAndHeight();
}

void SwTextFrame::SwitchLTRtoRTL( SwRect& rRect ) const
{
    SwSwapIfNotSwapped swap(const_cast<SwTextFrame *>(this));

    tools::Long nWidth = rRect.Width();
    rRect.Left( 2 * ( getFrameArea().Left() + getFramePrintArea().Left() ) +
                getFramePrintArea().Width() - rRect.Right() - 1 );

    rRect.Width( nWidth );
}

void SwTextFrame::SwitchLTRtoRTL( Point& rPoint ) const
{
    SwSwapIfNotSwapped swap(const_cast<SwTextFrame *>(this));

    rPoint.setX( 2 * ( getFrameArea().Left() + getFramePrintArea().Left() ) + getFramePrintArea().Width() - rPoint.X() - 1 );
}

SwLayoutModeModifier::SwLayoutModeModifier( const OutputDevice& rOutp ) :
        m_rOut( rOutp ), m_nOldLayoutMode( rOutp.GetLayoutMode() )
{
}

SwLayoutModeModifier::~SwLayoutModeModifier()
{
    const_cast<OutputDevice&>(m_rOut).SetLayoutMode( m_nOldLayoutMode );
}

void SwLayoutModeModifier::Modify( bool bChgToRTL )
{
    const_cast<OutputDevice&>(m_rOut).SetLayoutMode( bChgToRTL ?
                                         vcl::text::ComplexTextLayoutFlags::BiDiStrong | vcl::text::ComplexTextLayoutFlags::BiDiRtl :
                                         vcl::text::ComplexTextLayoutFlags::BiDiStrong );
}

void SwLayoutModeModifier::SetAuto()
{
    const vcl::text::ComplexTextLayoutFlags nNewLayoutMode = m_nOldLayoutMode & ~vcl::text::ComplexTextLayoutFlags::BiDiStrong;
    const_cast<OutputDevice&>(m_rOut).SetLayoutMode( nNewLayoutMode );
}

SwDigitModeModifier::SwDigitModeModifier( const OutputDevice& rOutp, LanguageType eCurLang,
                                          SvtCTLOptions::TextNumerals eCTLTextNumerals ) :
        rOut( rOutp ), nOldLanguageType( rOutp.GetDigitLanguage() )
{
    LanguageType eLang = eCurLang;
    if (comphelper::IsFuzzing())
        eLang = LANGUAGE_ENGLISH_US;
    else
    {
        if ( SvtCTLOptions::NUMERALS_HINDI == eCTLTextNumerals )
            eLang = LANGUAGE_ARABIC_SAUDI_ARABIA;
        else if ( SvtCTLOptions::NUMERALS_ARABIC == eCTLTextNumerals )
            eLang = LANGUAGE_ENGLISH;
        else if ( SvtCTLOptions::NUMERALS_SYSTEM == eCTLTextNumerals )
            eLang = ::GetAppLanguage();
    }

    const_cast<OutputDevice&>(rOut).SetDigitLanguage( eLang );
}

SwDigitModeModifier::~SwDigitModeModifier()
{
    const_cast<OutputDevice&>(rOut).SetDigitLanguage( nOldLanguageType );
}

void SwTextFrame::Init()
{
    OSL_ENSURE( !IsLocked(), "+SwTextFrame::Init: this is locked." );
    if( !IsLocked() )
    {
        ClearPara();
        SetHasRotatedPortions(false);
        // set flags directly to save a ResetPreps call,
        // and thereby an unnecessary GetPara call
        // don't set bOrphan, bLocked or bWait to false!
        // bOrphan = bFlag7 = bFlag8 = false;
    }
}

SwTextFrame::SwTextFrame(SwTextNode * const pNode, SwFrame* pSib,
        sw::FrameMode const eMode)
    : SwContentFrame( pNode, pSib )
    , mnAllLines( 0 )
    , mnThisLines( 0 )
    , mnFlyAnchorOfst( 0 )
    , mnFlyAnchorOfstNoWrap( 0 )
    , mnFlyAnchorVertOfstNoWrap( 0 )
    , mnFootnoteLine( 0 )
    , mnHeightOfLastLine( 0 )
    , mnAdditionalFirstLineOffset( 0 )
    , mnOffset( 0 )
    , mnNoHyphOffset( COMPLETE_STRING )
    , mnNoHyphEndZone( 0 )
    , mnCacheIndex( USHRT_MAX )
    , mbLocked( false )
    , mbWidow( false )
    , mbJustWidow( false )
    , mbEmpty( false )
    , mbInFootnoteConnect( false )
    , mbFootnote( false )
    , mbRepaint( false )
    , mbHasRotatedPortions( false )
    , mbFieldFollow( false )
    , mbHasAnimation( false )
    , mbIsSwapped( false )
    , mbFollowFormatAllowed( true )
{
    mnFrameType = SwFrameType::Txt;
    // note: this may call SwClientNotify if it's in a list so do it last
    // note: this may change this->pRegisteredIn to m_pMergedPara->listeners
    m_pMergedPara = CheckParaRedlineMerge(*this, *pNode, eMode);
}

void SwTextFrame::dumpAsXmlAttributes(xmlTextWriterPtr writer) const
{
    SwContentFrame::dumpAsXmlAttributes(writer);

    const SwTextNode *pTextNode = GetTextNodeFirst();
    (void)xmlTextWriterWriteFormatAttribute( writer, BAD_CAST( "txtNodeIndex" ), "%" SAL_PRIdINT32, sal_Int32(pTextNode->GetIndex()) );

    OString aMode = "Horizontal"_ostr;
    if (IsVertLRBT())
    {
        aMode = "VertBTLR"_ostr;
    }
    else if (IsVertLR())
    {
        aMode = "VertLR"_ostr;
    }
    else if (IsVertical())
    {
        aMode = "Vertical"_ostr;
    }
    (void)xmlTextWriterWriteAttribute(writer, BAD_CAST("WritingMode"), BAD_CAST(aMode.getStr()));
}

void SwTextFrame::dumpAsXml(xmlTextWriterPtr writer) const
{
    (void)xmlTextWriterStartElement(writer, reinterpret_cast<const xmlChar*>("txt"));
    dumpAsXmlAttributes( writer );
    if ( HasFollow() )
        (void)xmlTextWriterWriteFormatAttribute( writer, BAD_CAST( "follow" ), "%" SAL_PRIuUINT32, GetFollow()->GetFrameId() );

    if (m_pPrecede != nullptr)
        (void)xmlTextWriterWriteFormatAttribute( writer, BAD_CAST( "precede" ), "%" SAL_PRIuUINT32, static_cast<SwTextFrame*>(m_pPrecede)->GetFrameId() );

    (void)xmlTextWriterWriteAttribute(writer, BAD_CAST("offset"), BAD_CAST(OString::number(static_cast<sal_Int32>(mnOffset)).getStr()));

    sw::MergedPara const*const pMerged(GetMergedPara());
    if (pMerged)
    {
        (void)xmlTextWriterStartElement( writer, BAD_CAST( "merged" ) );
        (void)xmlTextWriterWriteFormatAttribute( writer, BAD_CAST( "paraPropsNodeIndex" ), "%" SAL_PRIdINT32, sal_Int32(pMerged->pParaPropsNode->GetIndex()) );
        for (auto const& e : pMerged->extents)
        {
            (void)xmlTextWriterStartElement( writer, BAD_CAST( "extent" ) );
            (void)xmlTextWriterWriteFormatAttribute( writer, BAD_CAST( "txtNodeIndex" ), "%" SAL_PRIdINT32, sal_Int32(e.pNode->GetIndex()) );
            (void)xmlTextWriterWriteFormatAttribute( writer, BAD_CAST( "start" ), "%" SAL_PRIdINT32, e.nStart );
            (void)xmlTextWriterWriteFormatAttribute( writer, BAD_CAST( "end" ), "%" SAL_PRIdINT32, e.nEnd );
            (void)xmlTextWriterEndElement( writer );
        }
        (void)xmlTextWriterEndElement( writer );
    }

    (void)xmlTextWriterStartElement(writer, BAD_CAST("infos"));
    dumpInfosAsXml(writer);
    (void)xmlTextWriterEndElement(writer);

    // Dump Anchored objects if any
    const SwSortedObjs* pAnchored = GetDrawObjs();
    if ( pAnchored && pAnchored->size() > 0 )
    {
        (void)xmlTextWriterStartElement( writer, BAD_CAST( "anchored" ) );

        for (SwAnchoredObject* pObject : *pAnchored)
        {
            pObject->dumpAsXml( writer );
        }

        (void)xmlTextWriterEndElement( writer );
    }

    // Dump the children
    OUString aText = GetText(  );
    for ( int i = 0; i < 32; i++ )
    {
        aText = aText.replace( i, '*' );
    }
    auto nTextOffset = static_cast<sal_Int32>(GetOffset());
    sal_Int32 nTextLength = aText.getLength() - nTextOffset;
    if (const SwTextFrame* pTextFrameFollow = GetFollow())
    {
        nTextLength = static_cast<sal_Int32>(pTextFrameFollow->GetOffset() - GetOffset());
    }
    if (nTextLength > 0)
    {
        OString aText8
            = OUStringToOString(aText.subView(nTextOffset, nTextLength), RTL_TEXTENCODING_UTF8);
        (void)xmlTextWriterWriteString( writer,
                reinterpret_cast<const xmlChar *>(aText8.getStr(  )) );
    }
    if (const SwParaPortion* pPara = GetPara())
    {
        (void)xmlTextWriterStartElement(writer, BAD_CAST("SwParaPortion"));
        TextFrameIndex nOffset(0);
        const OUString& rText = GetText();
        (void)xmlTextWriterWriteFormatAttribute(writer, BAD_CAST("ptr"), "%p", pPara);
        const SwLineLayout* pLine = pPara;
        if (IsFollow())
        {
            nOffset += GetOffset();
        }
        while (pLine)
        {
            (void)xmlTextWriterStartElement(writer, BAD_CAST("SwLineLayout"));
            pLine->dumpAsXmlAttributes(writer, rText, nOffset);
            const SwLinePortion* pPor = pLine->GetFirstPortion();
            while (pPor)
            {
                pPor->dumpAsXml(writer, rText, nOffset);
                pPor = pPor->GetNextPortion();
            }
            (void)xmlTextWriterEndElement(writer);
            pLine = pLine->GetNext();
        }
        (void)xmlTextWriterEndElement(writer);
    }

    (void)xmlTextWriterEndElement(writer);
}

namespace sw {

SwTextFrame * MakeTextFrame(SwTextNode & rNode, SwFrame *const pSibling,
        sw::FrameMode const eMode)
{
    return new SwTextFrame(&rNode, pSibling, eMode);
}

void RemoveFootnotesForNode(
        SwRootFrame const& rLayout, SwTextNode const& rTextNode,
        std::vector<std::pair<sal_Int32, sal_Int32>> const*const pExtents)
{
    if (pExtents && pExtents->empty())
    {
        return; // nothing to do
    }
    const SwFootnoteIdxs &rFootnoteIdxs = rTextNode.GetDoc().GetFootnoteIdxs();
    size_t nPos = 0;
    SwNodeOffset const nIndex = rTextNode.GetIndex();
    rFootnoteIdxs.SeekEntry( rTextNode, &nPos );
    if (nPos < rFootnoteIdxs.size())
    {
        while (nPos > 0 && rTextNode == (rFootnoteIdxs[ nPos ]->GetTextNode()))
            --nPos;
        if (nPos || rTextNode != (rFootnoteIdxs[ nPos ]->GetTextNode()))
            ++nPos;
    }
    size_t iter(0);
    for ( ; nPos < rFootnoteIdxs.size(); ++nPos)
    {
        SwTextFootnote* pTextFootnote = rFootnoteIdxs[ nPos ];
        if (pTextFootnote->GetTextNode().GetIndex() > nIndex)
            break;
        if (pExtents)
        {
            while ((*pExtents)[iter].second <= pTextFootnote->GetStart())
            {
                ++iter;
                if (iter == pExtents->size())
                {
                    return;
                }
            }
            if (pTextFootnote->GetStart() < (*pExtents)[iter].first)
            {
                continue;
            }
        }
        pTextFootnote->DelFrames(&rLayout);
    }
}

} // namespace sw

void SwTextFrame::DestroyImpl()
{
    // Remove associated SwParaPortion from s_pTextCache
    ClearPara();

    assert(!GetDoc().IsInDtor()); // this shouldn't be happening with ViewShell owning layout
    if (!GetDoc().IsInDtor() && HasFootnote())
    {
        if (m_pMergedPara)
        {
            SwTextNode const* pNode(nullptr);
            for (auto const& e : m_pMergedPara->extents)
            {
                if (e.pNode != pNode)
                {
                    pNode = e.pNode;
                    // sw_redlinehide: not sure if it's necessary to check
                    // if the nodes are still alive here, which would require
                    // accessing WriterMultiListener::m_vDepends
                    sw::RemoveFootnotesForNode(*getRootFrame(), *pNode, nullptr);
                }
            }
        }
        else
        {
            SwTextNode *const pNode(static_cast<SwTextNode*>(GetDep()));
            if (pNode)
            {
                sw::RemoveFootnotesForNode(*getRootFrame(), *pNode, nullptr);
            }
        }
    }

    if (!GetDoc().IsInDtor())
    {
        if (SwView* pView = GetActiveView())
            pView->GetEditWin().GetFrameControlsManager().RemoveControls(this);
    }

    SwContentFrame::DestroyImpl();
}

SwTextFrame::~SwTextFrame()
{
    RemoveFromCache();
}

namespace sw {

// 1. if real insert => correct nStart/nEnd for full nLen
// 2. if rl un-delete => do not correct nStart/nEnd but just include un-deleted
static TextFrameIndex UpdateMergedParaForInsert(MergedPara & rMerged,
        sw::ParagraphBreakMode const eMode, SwScriptInfo *const pScriptInfo,
        bool const isRealInsert,
        SwTextNode const& rNode, sal_Int32 const nIndex, sal_Int32 const nLen)
{
    assert(!isRealInsert || nLen); // can 0 happen? yes, for redline in empty node
    assert(nIndex <= rNode.Len());
    assert(nIndex + nLen <= rNode.Len());
    assert(rMerged.pFirstNode->GetIndex() <= rNode.GetIndex() && rNode.GetIndex() <= rMerged.pLastNode->GetIndex());
    if (!nLen)
    {
        return TextFrameIndex(0);
    }
    OUStringBuffer text(rMerged.mergedText);
    sal_Int32 nTFIndex(0); // index used for insertion at the end
    sal_Int32 nInserted(0);
    bool bInserted(false);
    bool bFoundNode(false);
    auto itInsert(rMerged.extents.end());
    for (auto it = rMerged.extents.begin(); it != rMerged.extents.end(); ++it)
    {
        if (it->pNode == &rNode)
        {
            if (isRealInsert)
            {
                bFoundNode = true;
                if (it->nStart <= nIndex && nIndex <= it->nEnd)
                {   // note: this can happen only once
                    text.insert(nTFIndex + (nIndex - it->nStart),
                            rNode.GetText().subView(nIndex, nLen));
                    it->nEnd += nLen;
                    nInserted = nLen;
                    assert(!bInserted);
                    bInserted = true;
                }
                else if (nIndex < it->nStart)
                {
                    if (itInsert == rMerged.extents.end())
                    {
                        itInsert = it;
                    }
                    it->nStart += nLen;
                    it->nEnd += nLen;
                }
            }
            else
            {
                assert(it == rMerged.extents.begin() || (it-1)->pNode != &rNode || (it-1)->nEnd < nIndex);
                if (nIndex + nLen < it->nStart)
                {
                    itInsert = it;
                    break;
                }
                if (nIndex < it->nStart)
                {
                    text.insert(nTFIndex,
                        rNode.GetText().subView(nIndex, it->nStart - nIndex));
                    nInserted += it->nStart - nIndex;
                    it->nStart = nIndex;
                    bInserted = true;
                }
                assert(it->nStart <= nIndex);
                if (nIndex <= it->nEnd)
                {
                    nTFIndex += it->nEnd - it->nStart;
                    while (it->nEnd < nIndex + nLen)
                    {
                        auto *const pNext(
                            (it+1) != rMerged.extents.end() && (it+1)->pNode == it->pNode
                                ? &*(it+1)
                                : nullptr);
                        if (pNext && pNext->nStart <= nIndex + nLen)
                        {
                            text.insert(nTFIndex,
                                rNode.GetText().subView(it->nEnd, pNext->nStart - it->nEnd));
                            nTFIndex += pNext->nStart - it->nEnd;
                            nInserted += pNext->nStart - it->nEnd;
                            pNext->nStart = it->nStart;
                            it = rMerged.extents.erase(it);
                        }
                        else
                        {
                            text.insert(nTFIndex,
                                rNode.GetText().subView(it->nEnd, nIndex + nLen - it->nEnd));
                            nTFIndex += nIndex + nLen - it->nEnd;
                            nInserted += nIndex + nLen - it->nEnd;
                            it->nEnd = nIndex + nLen;
                        }
                    }
                    bInserted = true;
                    break;
                }
            }
        }
        else if (rNode.GetIndex() < it->pNode->GetIndex() || bFoundNode)
        {
            if (itInsert == rMerged.extents.end())
            {
                itInsert = it;
            }
            break;
        }
        if (itInsert == rMerged.extents.end())
        {
            nTFIndex += it->nEnd - it->nStart;
        }
    }
//    assert((bFoundNode || rMerged.extents.empty()) && "text node not found - why is it sending hints to us");
    if (!bInserted)
    {   // must be in a gap
        rMerged.extents.emplace(itInsert, const_cast<SwTextNode*>(&rNode), nIndex, nIndex + nLen);
        text.insert(nTFIndex, rNode.GetText().subView(nIndex, nLen));
        nInserted = nLen;
        // called from SwRangeRedline::InvalidateRange()
        if (rNode.GetRedlineMergeFlag() == SwNode::Merge::Hidden)
        {
            const_cast<SwTextNode&>(rNode).SetRedlineMergeFlag(SwNode::Merge::NonFirst);
        }
    }
    rMerged.mergedText = text.makeStringAndClear();
    if ((!bInserted && rMerged.extents.size() == 1) // also if it was empty!
        || rNode.GetIndex() <= rMerged.pParaPropsNode->GetIndex())
    {   // text inserted before current para-props node
        SwTextNode *const pOldParaPropsNode{rMerged.pParaPropsNode};
        FindParaPropsNodeIgnoreHidden(rMerged, eMode, pScriptInfo);
        if (rMerged.pParaPropsNode != pOldParaPropsNode)
        {
            pOldParaPropsNode->RemoveFromListRLHidden();
            rMerged.pParaPropsNode->AddToListRLHidden();
        }
    }
    return TextFrameIndex(nInserted);
}

// 1. if real delete => correct nStart/nEnd for full nLen
// 2. if rl delete => do not correct nStart/nEnd but just exclude deleted
TextFrameIndex UpdateMergedParaForDelete(MergedPara & rMerged,
        sw::ParagraphBreakMode const eMode, SwScriptInfo *const pScriptInfo,
        bool const isRealDelete,
        SwTextNode const& rNode, sal_Int32 nIndex, sal_Int32 const nLen)
{
    assert(nIndex <= rNode.Len());
    assert(rMerged.pFirstNode->GetIndex() <= rNode.GetIndex() && rNode.GetIndex() <= rMerged.pLastNode->GetIndex());
    OUStringBuffer text(rMerged.mergedText);
    sal_Int32 nTFIndex(0);
    sal_Int32 nToDelete(nLen);
    sal_Int32 nDeleted(0);
    size_t nFoundNode(0);
//    size_t nErased(0);
    auto it = rMerged.extents.begin();
    for (; it != rMerged.extents.end(); )
    {
        bool bErase(false);
        if (it->pNode == &rNode)
        {
            ++nFoundNode;
            if (nIndex + nToDelete < it->nStart)
            {
                nToDelete = 0;
                if (!isRealDelete)
                {
                    break;
                }
                it->nStart -= nLen;
                it->nEnd -= nLen;
            }
            else
            {
                if (nIndex < it->nStart)
                {
                    // do not adjust nIndex into the text frame index space!
                    nToDelete -= it->nStart - nIndex;
                    nIndex = it->nStart;
                    // note: continue with the if check below, no else!
                }
                if (it->nStart <= nIndex && nIndex < it->nEnd)
                {
                    sal_Int32 const nDeleteHere(nIndex + nToDelete <= it->nEnd
                            ? nToDelete
                            : it->nEnd - nIndex);
                    text.remove(nTFIndex + (nIndex - it->nStart), nDeleteHere);
                    bErase = nDeleteHere == it->nEnd - it->nStart;
                    if (bErase)
                    {
//                        ++nErased;
                        assert(it->nStart == nIndex);
                        it = rMerged.extents.erase(it);
                    }
                    else if (isRealDelete)
                    {   // adjust for deleted text
                        it->nStart -= (nLen - nToDelete);
                        it->nEnd -= (nLen - nToDelete + nDeleteHere);
                        if (it != rMerged.extents.begin()
                            && (it-1)->pNode == &rNode
                            && (it-1)->nEnd == it->nStart)
                        {   // merge adjacent extents
                            nTFIndex += it->nEnd - it->nStart;
                            (it-1)->nEnd = it->nEnd;
                            it = rMerged.extents.erase(it);
                            bErase = true; // skip increment
                        }
                    }
                    else
                    {   // exclude text marked as deleted
                        if (nIndex + nDeleteHere == it->nEnd)
                        {
                            it->nEnd -= nDeleteHere;
                        }
                        else
                        {
                            if (nIndex == it->nStart)
                            {
                                it->nStart += nDeleteHere;
                            }
                            else
                            {
                                sal_Int32 const nOldEnd(it->nEnd);
                                it->nEnd = nIndex;
                                it = rMerged.extents.emplace(it+1,
                                    it->pNode, nIndex + nDeleteHere, nOldEnd);
                            }
                            assert(nDeleteHere == nToDelete);
                        }
                    }
                    nDeleted += nDeleteHere;
                    nToDelete -= nDeleteHere;
                    nIndex += nDeleteHere;
                    if (!isRealDelete && nToDelete == 0)
                    {
                        break;
                    }
                }
            }
        }
        else if (nFoundNode != 0)
        {
            break;
        }
        if (!bErase)
        {
            nTFIndex += it->nEnd - it->nStart;
            ++it;
        }
    }
//    assert(nFoundNode != 0 && "text node not found - why is it sending hints to us");
    assert(nIndex <= rNode.Len() + nLen);
    // if there's a remaining deletion, it must be in gap at the end of the node
// can't do: might be last one in node was erased   assert(nLen == 0 || rMerged.empty() || (it-1)->nEnd <= nIndex);
    // note: if first node gets deleted then that must call DelFrames as
    // pFirstNode is never updated
    rMerged.mergedText = text.makeStringAndClear();
// could be all-hidden now so always check!    if (nErased && nErased == nFoundNode)
    {   // all visible text from node was erased
#if 1
        if (rMerged.pParaPropsNode == &rNode)
        {
            SwTextNode *const pOldParaPropsNode{rMerged.pParaPropsNode};
            FindParaPropsNodeIgnoreHidden(rMerged, eMode, pScriptInfo);
            if (rMerged.pParaPropsNode != pOldParaPropsNode)
            {
                pOldParaPropsNode->RemoveFromListRLHidden();
                rMerged.pParaPropsNode->AddToListRLHidden();
            }
        }
#endif
// NOPE must listen on all non-hidden nodes; particularly on pLastNode        rMerged.listener.EndListening(&const_cast<SwTextNode&>(rNode));
    }
    return TextFrameIndex(nDeleted);
}

std::pair<SwTextNode*, sal_Int32>
MapViewToModel(MergedPara const& rMerged, TextFrameIndex const i_nIndex)
{
    sal_Int32 nIndex(i_nIndex);
    sw::Extent const* pExtent(nullptr);
    for (const auto& rExt : rMerged.extents)
    {
        pExtent = &rExt;
        if (nIndex < (pExtent->nEnd - pExtent->nStart))
        {
            return std::make_pair(pExtent->pNode, pExtent->nStart + nIndex);
        }
        nIndex = nIndex - (pExtent->nEnd - pExtent->nStart);
    }
    assert(nIndex == 0 && "view index out of bounds");
    return pExtent
        ? std::make_pair(pExtent->pNode, pExtent->nEnd) //1-past-the-end index
        : std::make_pair(const_cast<SwTextNode*>(rMerged.pLastNode), rMerged.pLastNode->Len());
}

TextFrameIndex MapModelToView(MergedPara const& rMerged, SwTextNode const*const pNode, sal_Int32 const nIndex)
{
    assert(rMerged.pFirstNode->GetIndex() <= pNode->GetIndex()
        && pNode->GetIndex() <= rMerged.pLastNode->GetIndex());
    sal_Int32 nRet(0);
    bool bFoundNode(false);
    for (auto const& e : rMerged.extents)
    {
        if (pNode->GetIndex() < e.pNode->GetIndex())
        {
            return TextFrameIndex(nRet);
        }
        if (e.pNode == pNode)
        {
            if (e.nStart <= nIndex && nIndex <= e.nEnd)
            {
                return TextFrameIndex(nRet + (nIndex - e.nStart));
            }
            else if (nIndex < e.nStart)
            {
                // in gap before this extent => map to 0 here TODO???
                return TextFrameIndex(nRet);
            }
            bFoundNode = true;
        }
        else if (bFoundNode)
        {
            break;
        }
        nRet += e.nEnd - e.nStart;
    }
    if (bFoundNode)
    {
        // must be in a gap at the end of the node
        assert(nIndex <= pNode->Len());
        return TextFrameIndex(nRet);
    }
    else if (rMerged.extents.empty())
    {
        assert(nIndex <= pNode->Len());
        return TextFrameIndex(0);
    }
    return TextFrameIndex(rMerged.mergedText.getLength());
}

} // namespace sw

std::pair<SwTextNode*, sal_Int32>
SwTextFrame::MapViewToModel(TextFrameIndex const nIndex) const
{
//nope    assert(GetPara());
    sw::MergedPara const*const pMerged(GetMergedPara());
    if (pMerged)
    {
        return sw::MapViewToModel(*pMerged, nIndex);
    }
    else
    {
        return std::make_pair(static_cast<SwTextNode*>(const_cast<sw::BroadcastingModify*>(
                    SwFrame::GetDep())), sal_Int32(nIndex));
    }
}

SwPosition SwTextFrame::MapViewToModelPos(TextFrameIndex const nIndex) const
{
    std::pair<SwTextNode*, sal_Int32> const ret(MapViewToModel(nIndex));
    return SwPosition(*ret.first, ret.second);
}

TextFrameIndex SwTextFrame::MapModelToView(SwTextNode const*const pNode, sal_Int32 const nIndex) const
{
//nope    assert(GetPara());
    sw::MergedPara const*const pMerged(GetMergedPara());
    if (pMerged)
    {
        return sw::MapModelToView(*pMerged, pNode, nIndex);
    }
    else
    {
        return TextFrameIndex(nIndex);
    }
}

TextFrameIndex SwTextFrame::MapModelToViewPos(SwPosition const& rPos) const
{
    SwTextNode const*const pNode(rPos.GetNode().GetTextNode());
    sal_Int32 const nIndex(rPos.GetContentIndex());
    return MapModelToView(pNode, nIndex);
}

void SwTextFrame::SetMergedPara(std::unique_ptr<sw::MergedPara> p)
{
    SwTextNode *const pFirst(m_pMergedPara ? m_pMergedPara->pFirstNode : nullptr);
    m_pMergedPara = std::move(p);
    if (pFirst)
    {
        if (m_pMergedPara)
        {
            assert(pFirst == m_pMergedPara->pFirstNode);
        }
        else
        {
            pFirst->Add(*this); // must register at node again
        }
    }
    // postcondition: frame must be listening somewhere
    assert(m_pMergedPara || GetDep());
}

const OUString& SwTextFrame::GetText() const
{
//nope    assert(GetPara());
    sw::MergedPara const*const pMerged(GetMergedPara());
    if (pMerged)
        return pMerged->mergedText;
    else
        return static_cast<SwTextNode const*>(SwFrame::GetDep())->GetText();
}

SwTextNode const* SwTextFrame::GetTextNodeForParaProps() const
{
    // FIXME can GetPara be 0 ? yes... this is needed in  SwContentNotify::SwContentNotify() which is called before any formatting is started
//nope    assert(GetPara());
    sw::MergedPara const*const pMerged(GetMergedPara());
    if (pMerged)
    {
//        assert(pMerged->pFirstNode == pMerged->pParaPropsNode); // surprising news!
        return pMerged->pParaPropsNode;
    }
    else
        return static_cast<SwTextNode const*>(SwFrame::GetDep());
}

SwTextNode const* SwTextFrame::GetTextNodeForFirstText() const
{
    sw::MergedPara const*const pMerged(GetMergedPara());
    if (pMerged)
        return pMerged->extents.empty()
            ? pMerged->pFirstNode
            : pMerged->extents.front().pNode;
    else
        return static_cast<SwTextNode const*>(SwFrame::GetDep());
}

SwTextNode const* SwTextFrame::GetTextNodeFirst() const
{
//nope    assert(GetPara());
    sw::MergedPara const*const pMerged(GetMergedPara());
    if (pMerged)
        return pMerged->pFirstNode;
    else
        return static_cast<SwTextNode const*>(SwFrame::GetDep());
}

SwDoc const& SwTextFrame::GetDoc() const
{
    return GetTextNodeFirst()->GetDoc();
}

LanguageType SwTextFrame::GetLangOfChar(TextFrameIndex const nIndex,
        sal_uInt16 const nScript, bool const bNoChar, bool const bNoneIfNoHyphenation) const
{
    // a single character can be mapped uniquely!
    std::pair<SwTextNode const*, sal_Int32> const pos(MapViewToModel(nIndex));
    return pos.first->GetLang(pos.second, bNoChar ? 0 : 1, nScript, bNoneIfNoHyphenation);
}

void SwTextFrame::ResetPreps()
{
    if ( GetCacheIdx() != USHRT_MAX )
    {
        if (SwParaPortion *pPara = GetPara())
            pPara->ResetPreps();
    }
}

static auto FindCellFrame(SwFrame const* pLower) -> SwLayoutFrame const*
{
    while (pLower)
    {
        if (pLower->IsCellFrame())
        {
            return static_cast<SwLayoutFrame const*>(pLower);
        }
        pLower = pLower->GetUpper();
    }
    return nullptr;
}

bool SwTextFrame::IsHiddenNow() const
{
    SwFrameSwapper aSwapper( this, true );

    if( !getFrameArea().Width() && isFrameAreaDefinitionValid() && GetUpper()->isFrameAreaDefinitionValid() ) // invalid when stack overflows (StackHack)!
    {
//        OSL_FAIL( "SwTextFrame::IsHiddenNow: thin frame" );
        return true;
    }

    // TODO: what is the above check good for and can it be removed?
    return IsHiddenNowImpl();
}

bool SwTextFrame::IsHiddenNowImpl() const
{
    if (SwContentFrame::IsHiddenNow())
        return true;

    bool bHiddenCharsHidePara(false);
    bool bHiddenParaField(false);
    if (m_pMergedPara)
    {
        TextFrameIndex nHiddenStart(COMPLETE_STRING);
        TextFrameIndex nHiddenEnd(0);
        bool hasHidden{false};
        if (auto const pScriptInfo = GetScriptInfo())
        {
            hasHidden = pScriptInfo->GetBoundsOfHiddenRange(TextFrameIndex(0),
                    nHiddenStart, nHiddenEnd);
        }
        else // ParaPortion is created in Format, but this is called earlier
        {
            SwScriptInfo aInfo;
            aInfo.InitScriptInfoHidden(*m_pMergedPara->pFirstNode, m_pMergedPara.get());
            hasHidden = aInfo.GetBoundsOfHiddenRange(TextFrameIndex(0),
                        nHiddenStart, nHiddenEnd);
        }
        if ((TextFrameIndex(0) == nHiddenStart
                && TextFrameIndex(GetText().getLength()) <= nHiddenEnd)
            // special case: GetBoundsOfHiddenRange doesn't assign!
            // but it does return that there *is* something hidden, in case
            // the frame is empty then the whole thing must be hidden
            || (hasHidden && m_pMergedPara->mergedText.isEmpty()))
        {
            bHiddenCharsHidePara = true;
        }
        sw::MergedAttrIter iter(*this);
        SwTextNode const* pNode(nullptr);
        int nNewResultWeight = 0;
        for (SwTextAttr const* pHint = iter.NextAttr(&pNode); pHint; pHint = iter.NextAttr(&pNode))
        {
            if (pHint->Which() == RES_TXTATR_FIELD)
            {
                // see also SwpHints::CalcHiddenParaField()
                const SwFormatField& rField = pHint->GetFormatField();
                int nCurWeight = pNode->GetDoc().FieldCanHideParaWeight(rField.GetField()->GetTyp()->Which());
                if (nCurWeight > nNewResultWeight)
                {
                    nNewResultWeight = nCurWeight;
                    bHiddenParaField = pNode->GetDoc().FieldHidesPara(*rField.GetField());
                }
                else if (nCurWeight == nNewResultWeight && bHiddenParaField)
                {
                    // Currently, for both supported hiding types (HiddenPara, Database), "Don't hide"
                    // takes precedence - i.e., if there's a "Don't hide" field of that weight, we only
                    // care about fields of higher weight.
                    bHiddenParaField = pNode->GetDoc().FieldHidesPara(*rField.GetField());
                }
            }
        }
    }
    else
    {
        bHiddenCharsHidePara = static_cast<SwTextNode const*>(SwFrame::GetDep())->HasHiddenCharAttribute( true );
        bHiddenParaField = static_cast<SwTextNode const*>(SwFrame::GetDep())->IsHiddenByParaField();
    }
    if (bHiddenCharsHidePara && GetDoc().getIDocumentSettingAccess().get(
            DocumentSettingId::APPLY_PARAGRAPH_MARK_FORMAT_TO_EMPTY_LINE_AT_END_OF_PARAGRAPH))
    {
        // apparently in Word it's always the last para marker that determines hidden?
        // even in case when they are merged by delete redline (it's obvious when they are merged by hidden-attribute
        SwTextNode const*const pNode{ m_pMergedPara
            ? m_pMergedPara->pLastNode
            : static_cast<SwTextNode const*>(SwFrame::GetDep()) };
        // Word ignores hidden formatting on the cell end marker
        bool isLastInCell{false};
        if (SwLayoutFrame const*const pCellFrame{FindCellFrame(this)})
        {
            SwContentFrame const* pNext{GetNextContentFrame()};
            // skip frame in hidden section ("this" is *not* in hidden section!)
            while (pNext && pNext->SwContentFrame::IsHiddenNow())
            {
                pNext = pNext->GetNextContentFrame();
            }
            isLastInCell = pNext == nullptr || !pCellFrame->IsAnLower(pNext);
        }
        if (!isLastInCell)
        {
            SwFormatAutoFormat const& rListAutoFormat{pNode->GetAttr(RES_PARATR_LIST_AUTOFMT)};
            std::shared_ptr<SfxItemSet> const pSet{rListAutoFormat.GetStyleHandle()};
            SvxCharHiddenItem const* pItem{pSet ? pSet->GetItemIfSet(RES_CHRATR_HIDDEN) : nullptr};
            if (!pItem)
            {
                // don't use node's mpAttrSet, it doesn't apply to para marker
                SwFormatColl const*const pStyle{pNode->GetFormatColl()};
                if (pStyle)
                {
                    pItem = &pStyle->GetFormatAttr(RES_CHRATR_HIDDEN);
                }
            }
            if (!pItem || !pItem->GetValue())
            {
                bHiddenCharsHidePara = false;
            }
        }
    }
    const SwViewShell* pVsh = getRootFrame()->GetCurrShell();

    if ( pVsh && ( bHiddenCharsHidePara || bHiddenParaField ) )
    {

        if (
             ( bHiddenParaField &&
               ( !pVsh->GetViewOptions()->IsShowHiddenPara() &&
                 !pVsh->GetViewOptions()->IsFieldName() ) ) ||
             ( bHiddenCharsHidePara &&
               !pVsh->GetViewOptions()->IsShowHiddenChar() ) )
        {
            // in order to put the cursor in the body text, one paragraph must
            // be visible - check this for the 1st body paragraph
            if (IsInDocBody() && FindPrevCnt() == nullptr)
            {
                for (SwContentFrame const* pNext = FindNextCnt(true);
                        pNext != nullptr; pNext = pNext->FindNextCnt(true))
                {
                    if (!pNext->IsHiddenNow())
                        return true;
                }
                SAL_INFO("sw.core", "unhiding one body paragraph");
                return false;
            }
            return true;
        }
    }

    return false;
}

/// Removes Textfrm's attachments, when it's hidden
void SwTextFrame::HideHidden()
{
    OSL_ENSURE( !GetFollow() && IsHiddenNow(),
            "HideHidden on visible frame of hidden frame has follow" );

    HideFootnotes(GetOffset(), TextFrameIndex(COMPLETE_STRING));
    HideAndShowObjects();

    // format information is obsolete
    ClearPara();
}

void SwTextFrame::HideFootnotes(TextFrameIndex const nStart, TextFrameIndex const nEnd)
{
    SwPageFrame *pPage = nullptr;
    sw::MergedAttrIter iter(*this);
    SwTextNode const* pNode(nullptr);
    for (SwTextAttr const* pHt = iter.NextAttr(&pNode); pHt; pHt = iter.NextAttr(&pNode))
    {
        if (pHt->Which() == RES_TXTATR_FTN)
        {
            TextFrameIndex const nIdx(MapModelToView(pNode, pHt->GetStart()));
            if (nEnd < nIdx)
                break;
            if (nStart <= nIdx)
            {
                if (!pPage)
                    pPage = FindPageFrame();
                pPage->RemoveFootnote( this, static_cast<const SwTextFootnote*>(pHt) );
            }
        }
    }
}

/**
 * as-character anchored graphics, which are used for a graphic bullet list.
 * As long as these graphic bullet list aren't imported, do not hide a
 * at-character anchored object, if
 * (a) the document is an imported WW8 document -
 *     checked by checking certain compatibility options -
 * (b) the paragraph is the last content in the document and
 * (c) the anchor character is an as-character anchored graphic.
 */
bool sw_HideObj( const SwTextFrame& _rFrame,
                  const RndStdIds _eAnchorType,
                  SwFormatAnchor const& rFormatAnchor,
                  SwAnchoredObject* _pAnchoredObj )
{
    bool bRet( true );

    if (_eAnchorType == RndStdIds::FLY_AT_CHAR)
    {
        const IDocumentSettingAccess *const pIDSA = &_rFrame.GetDoc().getIDocumentSettingAccess();
        if ( !pIDSA->get(DocumentSettingId::USE_FORMER_TEXT_WRAPPING) &&
             !pIDSA->get(DocumentSettingId::OLD_LINE_SPACING) &&
             !pIDSA->get(DocumentSettingId::USE_FORMER_OBJECT_POS) &&
              pIDSA->get(DocumentSettingId::CONSIDER_WRAP_ON_OBJECT_POSITION) &&
             _rFrame.IsInDocBody() && !_rFrame.FindNextCnt() )
        {
            SwTextNode const& rNode(*rFormatAnchor.GetAnchorNode()->GetTextNode());
            assert(FrameContainsNode(_rFrame, rNode.GetIndex()));
            sal_Int32 const nObjAnchorPos(rFormatAnchor.GetAnchorContentOffset());
            const sal_Unicode cAnchorChar = nObjAnchorPos < rNode.Len()
                ? rNode.GetText()[nObjAnchorPos]
                : 0;
            if (cAnchorChar == CH_TXTATR_BREAKWORD)
            {
                const SwTextAttr* const pHint(
                    rNode.GetTextAttrForCharAt(nObjAnchorPos, RES_TXTATR_FLYCNT));
                if ( pHint )
                {
                    const SwFrameFormat* pFrameFormat =
                        static_cast<const SwTextFlyCnt*>(pHint)->GetFlyCnt().GetFrameFormat();
                    if ( pFrameFormat->Which() == RES_FLYFRMFMT )
                    {
                        SwNodeIndex nContentIndex = *(pFrameFormat->GetContent().GetContentIdx());
                        ++nContentIndex;
                        if ( nContentIndex.GetNode().IsNoTextNode() )
                        {
                            bRet = false;
                            // set needed data structure values for object positioning
                            SwRectFnSet aRectFnSet(&_rFrame);
                            SwRect aLastCharRect( _rFrame.getFrameArea() );
                            aRectFnSet.SetWidth( aLastCharRect, 1 );
                            _pAnchoredObj->maLastCharRect = aLastCharRect;
                            _pAnchoredObj->mnLastTopOfLine = aRectFnSet.GetTop(aLastCharRect);
                        }
                    }
                }
            }
        }
    }

    return bRet;
}

/**
 * Hide/show objects
 *
 * Method hides respectively shows objects, which are anchored at paragraph,
 * at/as a character of the paragraph, corresponding to the paragraph and
 * paragraph portion visibility.
 *
 * - is called from HideHidden() - should hide objects in hidden paragraphs and
 * - from Format_() - should hide/show objects in partly visible paragraphs
 */
void SwTextFrame::HideAndShowObjects()
{
    if ( GetDrawObjs() )
    {
        if ( IsHiddenNow() )
        {
            // complete paragraph is hidden. Thus, hide all objects
            for (SwAnchoredObject* i : *GetDrawObjs())
            {
                SdrObject* pObj = i->DrawObj();
                SwContact* pContact = static_cast<SwContact*>(pObj->GetUserCall());
                // under certain conditions
                const RndStdIds eAnchorType( pContact->GetAnchorId() );
                if ((eAnchorType != RndStdIds::FLY_AT_CHAR) ||
                    sw_HideObj(*this, eAnchorType, pContact->GetAnchorFormat(),
                                 i ))
                {
                    pContact->MoveObjToInvisibleLayer( pObj );
                }
            }
        }
        else
        {
            // paragraph is visible, but can contain hidden text portion.
            // first we check if objects are allowed to be hidden:
            const SwViewShell* pVsh = getRootFrame()->GetCurrShell();
            const bool bShouldBeHidden = !pVsh || !pVsh->GetWin() ||
                                         !pVsh->GetViewOptions()->IsShowHiddenChar();

            // Thus, show all objects, which are anchored at paragraph and
            // hide/show objects, which are anchored at/as character, according
            // to the visibility of the anchor character.
            for (SwAnchoredObject* i : *GetDrawObjs())
            {
                SdrObject* pObj = i->DrawObj();
                SwContact* pContact = static_cast<SwContact*>(pObj->GetUserCall());
                // Determine anchor type only once
                const RndStdIds eAnchorType( pContact->GetAnchorId() );

                if (eAnchorType == RndStdIds::FLY_AT_PARA)
                {
                    pContact->MoveObjToVisibleLayer( pObj );
                }
                else if ((eAnchorType == RndStdIds::FLY_AT_CHAR) ||
                         (eAnchorType == RndStdIds::FLY_AS_CHAR))
                {
                    sal_Int32 nHiddenStart;
                    sal_Int32 nHiddenEnd;
                    const SwFormatAnchor& rAnchorFormat = pContact->GetAnchorFormat();
                    const SwNode* pNode = rAnchorFormat.GetAnchorNode();
                    // When the object was already removed from text, but the layout hasn't been
                    // updated yet, this can be nullptr:
                    if (!pNode)
                        continue;
                    SwScriptInfo::GetBoundsOfHiddenRange(
                        *pNode->GetTextNode(),
                        rAnchorFormat.GetAnchorContentOffset(), nHiddenStart, nHiddenEnd);
                    // Under certain conditions
                    if ( nHiddenStart != COMPLETE_STRING && bShouldBeHidden &&
                        sw_HideObj(*this, eAnchorType, rAnchorFormat, i))
                    {
                        pContact->MoveObjToInvisibleLayer( pObj );
                    }
                    else
                        pContact->MoveObjToVisibleLayer( pObj );
                }
                else
                {
                    OSL_FAIL( "<SwTextFrame::HideAndShowObjects()> - object not anchored at/inside paragraph!?" );
                }
            }
        }
    }

    if (IsFollow())
    {
        SwTextFrame *pMaster = FindMaster();
        OSL_ENSURE(pMaster, "SwTextFrame without master");
        if (pMaster)
            pMaster->HideAndShowObjects();
    }
}

void SwLayoutFrame::HideAndShowObjects()
{
    for (SwFrame * pLower = Lower(); pLower; pLower = pLower->GetNext())
    {
        pLower->HideAndShowObjects();
    }
}

void SwFrame::HideAndShowObjects()
{
}

/**
 * Returns the first possible break point in the current line.
 * This method is used in SwTextFrame::Format() to decide whether the previous
 * line has to be formatted as well.
 * nFound is <= nEndLine.
 */
TextFrameIndex SwTextFrame::FindBrk(std::u16string_view aText,
                              const TextFrameIndex nStart,
                              const TextFrameIndex nEnd)
{
    sal_Int32 nFound = sal_Int32(nStart);
    const sal_Int32 nEndLine = std::min(sal_Int32(nEnd), sal_Int32(aText.size()) - 1);

    // Skip all leading blanks.
    while( nFound <= nEndLine && ' ' == aText[nFound] )
    {
         nFound++;
    }

    // A tricky situation with the TextAttr-Dummy-character (in this case "$"):
    // "Dr.$Meyer" at the beginning of the second line. Typing a blank after that
    // doesn't result in the word moving into first line, even though that would work.
    // For this reason we don't skip the dummy char.
    while( nFound <= nEndLine && ' ' != aText[nFound] )
    {
        nFound++;
    }

    return TextFrameIndex(nFound);
}

bool SwTextFrame::IsIdxInside(TextFrameIndex const nPos, TextFrameIndex const nLen) const
{
    if (nPos == TextFrameIndex(COMPLETE_STRING)) // the "not found" range
        return false;
// Silence over-eager warning emitted at least by GCC trunk towards 6:
#if defined __GNUC__ && !defined __clang__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-overflow"
#endif
    if (nLen != TextFrameIndex(COMPLETE_STRING) && GetOffset() > nPos + nLen) // the range preceded us
#if defined __GNUC__ && !defined __clang__
#pragma GCC diagnostic pop
#endif
        return false;

    if( !GetFollow() ) // the range doesn't precede us,
        return true; // nobody follows us.

    TextFrameIndex const nMax = GetFollow()->GetOffset();

    // either the range overlap or our text has been deleted
    // sw_redlinehide: GetText() should be okay here because it has already
    // been updated in the INS/DEL hint case
    if (nMax > nPos || nMax > TextFrameIndex(GetText().getLength()))
        return true;

    // changes made in the first line of a follow can modify the master
    const SwParaPortion* pPara = GetFollow()->GetPara();
    return pPara && ( nPos <= nMax + pPara->GetLen() );
}

inline void SwTextFrame::InvalidateRange(const SwCharRange &aRange, const tools::Long nD)
{
    if ( IsIdxInside( aRange.Start(), aRange.Len() ) )
        InvalidateRange_( aRange, nD );
}

void SwTextFrame::InvalidateRange_( const SwCharRange &aRange, const tools::Long nD)
{
    if ( !HasPara() )
    {   InvalidateSize();
        return;
    }

    SetWidow( false );
    SwParaPortion *pPara = GetPara();

    bool bInv = false;
    if( 0 != nD )
    {
        // In nDelta the differences between old and new
        // linelengths are being added, that's why it's negative
        // if chars have been added and positive, if chars have
        // deleted
        pPara->SetDelta(pPara->GetDelta() + nD);
        bInv = true;
    }
    SwCharRange &rReformat = pPara->GetReformat();
    if(aRange != rReformat) {
        if (TextFrameIndex(COMPLETE_STRING) == rReformat.Len())
            rReformat = aRange;
        else
            rReformat += aRange;
        bInv = true;
    }
    if(bInv)
    {
        InvalidateSize();
    }
}

void SwTextFrame::CalcLineSpace()
{
    OSL_ENSURE( ! IsVertical() || ! IsSwapped(),
            "SwTextFrame::CalcLineSpace with swapped frame!" );

    if( IsLocked() || !HasPara() )
        return;

    if( GetDrawObjs() ||
        GetTextNodeForParaProps()->GetSwAttrSet().GetFirstLineIndent().IsAutoFirst())
    {
        Init();
        return;
    }

    SwParaPortion *const pPara(GetPara());
    assert(pPara);
    if (pPara->IsFixLineHeight())
    {
        Init();
        return;
    }

    Size aNewSize( getFramePrintArea().SSize() );

    SwTextFormatInfo aInf( getRootFrame()->GetCurrShell()->GetOut(), this );
    SwTextFormatter aLine( this, &aInf );
    if( aLine.GetDropLines() )
    {
        Init();
        return;
    }

    aLine.Top();
    aLine.RecalcRealHeight();

    aNewSize.setHeight( (aLine.Y() - getFrameArea().Top()) + aLine.GetLineHeight() );

    SwTwips nDelta = aNewSize.Height() - getFramePrintArea().Height();
    // Underflow with free-flying frames
    if( aInf.GetTextFly().IsOn() )
    {
        SwRect aTmpFrame( getFrameArea() );
        if( nDelta < 0 )
            aTmpFrame.Height( getFramePrintArea().Height() );
        else
            aTmpFrame.Height( aNewSize.Height() );
        if( aInf.GetTextFly().Relax( aTmpFrame ) )
        {
            Init();
            return;
        }
    }

    if( !nDelta )
        return;

    SwTextFrameBreak aBreak( this );
    if( GetFollow() || aBreak.IsBreakNow( aLine ) )
    {
        // if there is a Follow() or if we need to break here, reformat
        Init();
    }
    else
    {
        // everything is business as usual...
        pPara->SetPrepAdjust();
        pPara->SetPrep();
    }
}

static void lcl_SetWrong( SwTextFrame& rFrame, SwTextNode const& rNode,
        sal_Int32 const nPos, sal_Int32 const nCnt, bool const bMove)
{
    if ( !rFrame.IsFollow() )
    {
        SwTextNode& rTextNode = const_cast<SwTextNode&>(rNode);
        sw::GrammarContact* pGrammarContact = sw::getGrammarContactFor(rTextNode);
        SwGrammarMarkUp* pWrongGrammar = pGrammarContact ?
            pGrammarContact->getGrammarCheck( rTextNode, false ) :
            rTextNode.GetGrammarCheck();
        bool bGrammarProxy = pWrongGrammar != rTextNode.GetGrammarCheck();
        if( bMove )
        {
            if( rTextNode.GetWrong() )
                rTextNode.GetWrong()->Move( nPos, nCnt );
            if( pWrongGrammar )
                pWrongGrammar->MoveGrammar( nPos, nCnt );
            if( bGrammarProxy && rTextNode.GetGrammarCheck() )
                rTextNode.GetGrammarCheck()->MoveGrammar( nPos, nCnt );
            if( rTextNode.GetSmartTags() )
                rTextNode.GetSmartTags()->Move( nPos, nCnt );
        }
        else
        {
            if( rTextNode.GetWrong() )
                rTextNode.GetWrong()->Invalidate( nPos, nCnt );
            if( pWrongGrammar )
                pWrongGrammar->Invalidate( nPos, nCnt );
            if( rTextNode.GetSmartTags() )
                rTextNode.GetSmartTags()->Invalidate( nPos, nCnt );
        }
        const sal_Int32 nEnd = nPos + (nCnt > 0 ? nCnt : 1 );
        if ( !rTextNode.GetWrong() && !rTextNode.IsWrongDirty() )
        {
            rTextNode.SetWrong( std::make_unique<SwWrongList>( WRONGLIST_SPELL ) );
            rTextNode.GetWrong()->SetInvalid( nPos, nEnd );
        }
        if ( !rTextNode.GetSmartTags() && !rTextNode.IsSmartTagDirty() )
        {
            rTextNode.SetSmartTags( std::make_unique<SwWrongList>( WRONGLIST_SMARTTAG ) );
            rTextNode.GetSmartTags()->SetInvalid( nPos, nEnd );
        }
        rTextNode.SetWrongDirty(sw::WrongState::TODO);
        rTextNode.SetGrammarCheckDirty( true );
        rTextNode.SetWordCountDirty( true );
        rTextNode.SetAutoCompleteWordDirty( true );
        rTextNode.SetSmartTagDirty( true );
    }

    SwRootFrame *pRootFrame = rFrame.getRootFrame();
    if (pRootFrame)
    {
        pRootFrame->SetNeedGrammarCheck( true );
    }

    SwPageFrame *pPage = rFrame.FindPageFrame();
    if( pPage )
    {
        pPage->InvalidateSpelling();
        pPage->InvalidateAutoCompleteWords();
        pPage->InvalidateWordCount();
        pPage->InvalidateSmartTags();
    }
}

static void lcl_SetScriptInval(SwTextFrame& rFrame, TextFrameIndex const nPos)
{
    if( rFrame.GetPara() )
        rFrame.GetPara()->GetScriptInfo().SetInvalidityA( nPos );
}

// note: SwClientNotify will be called once for every frame => just fix own Ofst
static void lcl_ModifyOfst(SwTextFrame & rFrame,
        TextFrameIndex const nPos, TextFrameIndex const nLen,
        TextFrameIndex (* op)(TextFrameIndex const&, TextFrameIndex const&))
{
    assert(nLen != TextFrameIndex(COMPLETE_STRING));
    if (rFrame.IsFollow() && nPos < rFrame.GetOffset())
    {
        rFrame.ManipOfst( std::max(nPos, op(rFrame.GetOffset(), nLen)) );
        assert(sal_Int32(rFrame.GetOffset()) <= rFrame.GetText().getLength());
    }
}

namespace {

void UpdateMergedParaForMove(sw::MergedPara & rMerged,
        SwTextFrame & rTextFrame,
        bool & o_rbRecalcFootnoteFlag,
        SwTextNode const& rDestNode,
        SwTextNode const& rNode,
        sal_Int32 const nDestStart,
        sal_Int32 const nSourceStart,
        sal_Int32 const nLen)
{
    std::vector<std::pair<sal_Int32, sal_Int32>> deleted;
    sal_Int32 const nSourceEnd(nSourceStart + nLen);
    sal_Int32 nLastEnd(0);
    for (const auto& rExt : rMerged.extents)
    {
        if (rExt.pNode == &rNode)
        {
            sal_Int32 const nStart(std::max(nLastEnd, nSourceStart));
            sal_Int32 const nEnd(std::min(rExt.nStart, nSourceEnd));
            if (nStart < nEnd)
            {
                deleted.emplace_back(nStart, nEnd);
            }
            nLastEnd = rExt.nEnd;
            if (nSourceEnd <= rExt.nEnd)
            {
                break;
            }
        }
        else if (rNode.GetIndex() < rExt.pNode->GetIndex())
        {
            break;
        }
    }
    if (nLastEnd != rNode.Len()) // without nLen, string yet to be removed
    {
        if (nLastEnd < nSourceEnd)
        {
            deleted.emplace_back(std::max(nLastEnd, nSourceStart), nSourceEnd);
        }
    }
    if (deleted.empty())
        return;

    o_rbRecalcFootnoteFlag = true;
    for (auto const& it : deleted)
    {
        sal_Int32 const nStart(it.first - nSourceStart + nDestStart);
        TextFrameIndex const nDeleted = UpdateMergedParaForDelete(rMerged,
            rTextFrame.getRootFrame()->GetParagraphBreakMode(), rTextFrame.GetScriptInfo(),
            false, rDestNode, nStart, it.second - it.first);
//FIXME asserts valid for join - but if called from split, the new node isn't there yet and it will be added later...       assert(nDeleted);
//            assert(nDeleted == it.second - it.first);
        if(nDeleted)
        {
            // InvalidateRange/lcl_SetScriptInval was called sufficiently for InsertText
            lcl_SetWrong(rTextFrame, rDestNode, nStart, it.first - it.second, false);
            TextFrameIndex const nIndex(sw::MapModelToView(rMerged, &rDestNode, nStart));
            lcl_ModifyOfst(rTextFrame, nIndex, nDeleted, &o3tl::operator-<sal_Int32, Tag_TextFrameIndex>);
        }
    }
}

} // namespace

/**
 * Related: fdo#56031 filter out attribute changes that don't matter for
 * humans/a11y to stop flooding the destination mortal with useless noise
 */
#if !ENABLE_WASM_STRIP_ACCESSIBILITY
static bool isA11yRelevantAttribute(sal_uInt16 nWhich)
{
    return nWhich != RES_CHRATR_RSID;
}

static bool hasA11yRelevantAttribute( const std::vector<sal_uInt16>& rWhichFmtAttr )
{
    for( sal_uInt16 nWhich : rWhichFmtAttr )
        if ( isA11yRelevantAttribute( nWhich ) )
            return true;

    return false;
}
#endif // ENABLE_WASM_STRIP_ACCESSIBILITY

// Note: for now this overrides SwClient::SwClientNotify; the intermediary
// classes still override SwClient::Modify, which should continue to work
// as their implementation of SwClientNotify is SwClient's which calls Modify.
// Therefore we also don't need to call SwClient::SwClientNotify(rModify, rHint)
// because that's all it does, and this implementation calls
// SwContentFrame::SwClientNotify() when appropriate.
void SwTextFrame::SwClientNotify(SwModify const& rModify, SfxHint const& rHint)
{
    SfxPoolItem const* pOld(nullptr);
    SfxPoolItem const* pNew(nullptr);
    sw::MoveText const* pMoveText(nullptr);
    sw::InsertText const* pInsertText(nullptr);
    sw::DeleteText const* pDeleteText(nullptr);
    sw::DeleteChar const* pDeleteChar(nullptr);
    sw::RedlineDelText const* pRedlineDelText(nullptr);
    sw::RedlineUnDelText const* pRedlineUnDelText(nullptr);
    SwFormatChangeHint const * pFormatChangedHint(nullptr);
    sw::AttrSetChangeHint const* pAttrSetChangeHint(nullptr);
    sw::UpdateAttrHint const* pUpdateAttrHint(nullptr);

    sal_uInt16 nWhich = 0;
    if (rHint.GetId() == SfxHintId::SwLegacyModify)
    {
        auto pHint = static_cast<const sw::LegacyModifyHint*>(&rHint);
        pOld = pHint->m_pOld;
        pNew = pHint->m_pNew;
        nWhich = pHint->GetWhich();
    }
    else if (rHint.GetId() == SfxHintId::SwUpdateAttr)
    {
        pUpdateAttrHint = static_cast<const sw::UpdateAttrHint*>(&rHint);
    }
    else if (rHint.GetId() == SfxHintId::SwInsertText)
    {
        pInsertText = static_cast<const sw::InsertText*>(&rHint);
    }
    else if (rHint.GetId() == SfxHintId::SwDeleteText)
    {
        pDeleteText = static_cast<const sw::DeleteText*>(&rHint);
    }
    else if (rHint.GetId() == SfxHintId::SwDeleteChar)
    {
        pDeleteChar = static_cast<const sw::DeleteChar*>(&rHint);
    }
    else if (rHint.GetId() == SfxHintId::SwDocPosUpdateAtIndex)
    {
        auto pDocPosAt = static_cast<const sw::DocPosUpdateAtIndex*>(&rHint);
        Broadcast(SfxHint()); // notify SwAccessibleParagraph
        if(IsLocked())
            return;
        if(pDocPosAt->m_nDocPos > getFrameArea().Top())
            return;
        TextFrameIndex const nIndex(MapModelToView(
            &pDocPosAt->m_rNode,
            pDocPosAt->m_nIndex));
        InvalidateRange(SwCharRange(nIndex, TextFrameIndex(1)));
        return;
    }
    else if (rHint.GetId() == SfxHintId::SwVirtPageNumHint)
    {
        auto& rVirtPageNumHint = const_cast<sw::VirtPageNumHint&>(static_cast<const sw::VirtPageNumHint&>(rHint));
        if(!IsInDocBody() || IsFollow() || rVirtPageNumHint.IsFound())
            return;
        if(const SwPageFrame* pPage = FindPageFrame())
            pPage->UpdateVirtPageNumInfo(rVirtPageNumHint, this);
        return;
    }
    else if (rHint.GetId() == SfxHintId::SwMoveText)
    {
        pMoveText = static_cast<sw::MoveText const*>(&rHint);
    }
    else if (rHint.GetId() == SfxHintId::SwRedlineDelText)
    {
        pRedlineDelText = static_cast<sw::RedlineDelText const*>(&rHint);
    }
    else if (rHint.GetId() == SfxHintId::SwRedlineUnDelText)
    {
        pRedlineUnDelText = static_cast<sw::RedlineUnDelText const*>(&rHint);
    }
    else if (rHint.GetId() == SfxHintId::SwFormatChange)
    {
        pFormatChangedHint = static_cast<const SwFormatChangeHint*>(&rHint);
    }
    else if (rHint.GetId() == SfxHintId::SwAttrSetChange)
    {
        pAttrSetChangeHint = static_cast<const sw::AttrSetChangeHint*>(&rHint);
    }
    else
    {
        assert(!"unexpected hint");
    }

    if (m_pMergedPara)
    {
        assert(m_pMergedPara->listener.IsListeningTo(&rModify));
    }

    SwTextNode const& rNode(static_cast<SwTextNode const&>(rModify));

    // modifications concerning frame attributes are processed by the base class
    if( IsInRange( aFrameFormatSetRange, nWhich ) || pFormatChangedHint )
    {
        if (m_pMergedPara)
        {   // ignore item set changes that don't apply
            SwTextNode const*const pAttrNode(
                (nWhich == RES_PAGEDESC || nWhich == RES_BREAK)
                    ? m_pMergedPara->pFirstNode
                    : m_pMergedPara->pParaPropsNode);
            if (pAttrNode != &rModify)
            {
                return;
            }
        }
        SwContentFrame::SwClientNotify(rModify, rHint);
        if( pFormatChangedHint && getRootFrame()->GetCurrShell() )
        {
            // collection has changed
            Prepare();
            InvalidatePrt_();
            lcl_SetWrong( *this, rNode, 0, COMPLETE_STRING, false );
            SetDerivedR2L( false );
            CheckDirChange();
            // Force complete paint due to existing indents.
            SetCompletePaint();
            InvalidateLineNum();
        }
        return;
    }

    if (m_pMergedPara && m_pMergedPara->pParaPropsNode != &rModify)
    {
        if (isPARATR(nWhich) || isPARATR_LIST(nWhich)) // FRMATR handled above
        {
            return; // ignore it
        }
    }

    Broadcast(SfxHint()); // notify SwAccessibleParagraph

    // while locked ignore all modifications
    if( IsLocked() )
        return;

    // save stack
    // warning: one has to ensure that all variables are set
    TextFrameIndex nPos;
    TextFrameIndex nLen;
    bool bSetFieldsDirty = false;
    bool bRecalcFootnoteFlag = false;

    if (pRedlineDelText)
    {
        if (m_pMergedPara)
        {
            sal_Int32 const nNPos = pRedlineDelText->nStart;
            sal_Int32 const nNLen = pRedlineDelText->nLen;
            nPos = MapModelToView(&rNode, nNPos);
            // update merged before doing anything else
            nLen = UpdateMergedParaForDelete(*m_pMergedPara,
                    getRootFrame()->GetParagraphBreakMode(), GetScriptInfo(),
                    false, rNode, nNPos, nNLen);
            const sal_Int32 m = -nNLen;
            if (nLen && IsIdxInside(nPos, nLen))
            {
                InvalidateRange( SwCharRange(nPos, TextFrameIndex(1)), m );
            }
            lcl_SetWrong( *this, rNode, nNPos, m, false );
            if (nLen)
            {
                lcl_SetScriptInval( *this, nPos );
                bSetFieldsDirty = bRecalcFootnoteFlag = true;
                lcl_ModifyOfst(*this, nPos, nLen, &o3tl::operator-<sal_Int32, Tag_TextFrameIndex>);
            }
        }
    }
    else if (pRedlineUnDelText)
    {
        if (m_pMergedPara)
        {
            sal_Int32 const nNPos = pRedlineUnDelText->nStart;
            sal_Int32 const nNLen = pRedlineUnDelText->nLen;
            nPos = MapModelToView(&rNode, nNPos);
            nLen = UpdateMergedParaForInsert(*m_pMergedPara,
                    getRootFrame()->GetParagraphBreakMode(), GetScriptInfo(),
                    false, rNode, nNPos, nNLen);
            if (IsIdxInside(nPos, nLen))
            {
                if (!nLen)
                {
                    // Refresh NumPortions even when line is empty!
                    if (nPos)
                        InvalidateSize();
                    else
                        Prepare();
                }
                else
                    InvalidateRange_( SwCharRange( nPos, nLen ), nNLen );
            }
            lcl_SetWrong( *this, rNode, nNPos, nNLen, false );
            lcl_SetScriptInval( *this, nPos );
            bSetFieldsDirty = true;
            lcl_ModifyOfst(*this, nPos, nLen, &o3tl::operator+<sal_Int32, Tag_TextFrameIndex>);
        }
    }
    else if (pMoveText)
    {
        if (m_pMergedPara
            && m_pMergedPara->pFirstNode->GetIndex() <= pMoveText->pDestNode->GetIndex()
            && pMoveText->pDestNode->GetIndex() <= m_pMergedPara->pLastNode->GetIndex())
        {   // if it's not 2 nodes in merged frame, assume the target node doesn't have frames at all
            assert(abs(rNode.GetIndex() - pMoveText->pDestNode->GetIndex()) == SwNodeOffset(1));
            UpdateMergedParaForMove(*m_pMergedPara,
                    *this,
                    bRecalcFootnoteFlag,
                    *pMoveText->pDestNode, rNode,
                    pMoveText->nDestStart,
                    pMoveText->nSourceStart,
                    pMoveText->nLen);
        }
        else
        {
            // there is a situation where this is okay: from JoinNext, which will then call CheckResetRedlineMergeFlag, which will then create merged from scratch for this frame
            // assert(!m_pMergedPara || !getRootFrame()->IsHideRedlines() || !pMoveText->pDestNode->getLayoutFrame(getRootFrame()));
        }
    }
    else if (pInsertText)
    {
        nPos = MapModelToView(&rNode, pInsertText->nPos);
        // unlike redlines, inserting into fieldmark must be explicitly handled
        bool isHidden(false);
        switch (getRootFrame()->GetFieldmarkMode())
        {
            case sw::FieldmarkMode::ShowCommand:
                isHidden = pInsertText->isInsideFieldmarkResult;
            break;
            case sw::FieldmarkMode::ShowResult:
                isHidden = pInsertText->isInsideFieldmarkCommand;
            break;
            case sw::FieldmarkMode::ShowBoth: // just to avoid the warning
            break;
        }
        if (!isHidden)
        {
            nLen = TextFrameIndex(pInsertText->nLen);
            if (m_pMergedPara)
            {
                UpdateMergedParaForInsert(*m_pMergedPara,
                    getRootFrame()->GetParagraphBreakMode(), GetScriptInfo(),
                    true, rNode, pInsertText->nPos, pInsertText->nLen);
            }
            if( IsIdxInside( nPos, nLen ) )
            {
                if( !nLen )
                {
                    // Refresh NumPortions even when line is empty!
                    if( nPos )
                        InvalidateSize();
                    else
                        Prepare();
                }
                else
                    InvalidateRange_( SwCharRange( nPos, nLen ), pInsertText->nLen );
            }
            lcl_SetScriptInval( *this, nPos );
            bSetFieldsDirty = true;
            lcl_ModifyOfst(*this, nPos, nLen, &o3tl::operator+<sal_Int32, Tag_TextFrameIndex>);
        }
        lcl_SetWrong( *this, rNode, pInsertText->nPos, pInsertText->nLen, true );
    }
    else if (pDeleteText)
    {
        nPos = MapModelToView(&rNode, pDeleteText->nStart);
        if (m_pMergedPara)
        {   // update merged before doing anything else
            nLen = UpdateMergedParaForDelete(*m_pMergedPara,
                getRootFrame()->GetParagraphBreakMode(), GetScriptInfo(),
                true, rNode, pDeleteText->nStart, pDeleteText->nLen);
        }
        else
        {
            nLen = TextFrameIndex(pDeleteText->nLen);
        }
        const sal_Int32 m = -pDeleteText->nLen;
        if ((!m_pMergedPara || nLen) && IsIdxInside(nPos, nLen))
        {
            if( !nLen )
                InvalidateSize();
            else
                InvalidateRange( SwCharRange(nPos, TextFrameIndex(1)), m );
        }
        lcl_SetWrong( *this, rNode, pDeleteText->nStart, m, true );
        if (nLen)
        {
            lcl_SetScriptInval( *this, nPos );
            bSetFieldsDirty = bRecalcFootnoteFlag = true;
            lcl_ModifyOfst(*this, nPos, nLen, &o3tl::operator-<sal_Int32, Tag_TextFrameIndex>);
        }
    }
    else if (pDeleteChar)
    {
        nPos = MapModelToView(&rNode, pDeleteChar->m_nPos);
        if (m_pMergedPara)
        {
            nLen = UpdateMergedParaForDelete(*m_pMergedPara,
                getRootFrame()->GetParagraphBreakMode(), GetScriptInfo(),
                true, rNode, pDeleteChar->m_nPos, 1);
        }
        else
        {
            nLen = TextFrameIndex(1);
        }
        lcl_SetWrong( *this, rNode, pDeleteChar->m_nPos, -1, true );
        if (nLen)
        {
            InvalidateRange( SwCharRange(nPos, nLen), -1 );
            lcl_SetScriptInval( *this, nPos );
            bSetFieldsDirty = bRecalcFootnoteFlag = true;
            lcl_ModifyOfst(*this, nPos, nLen, &o3tl::operator-<sal_Int32, Tag_TextFrameIndex>);
        }
    }
    else if (pAttrSetChangeHint)
    {
        InvalidateLineNum();

        const SwAttrSet& rNewSet = *pAttrSetChangeHint->m_pNew->GetChgSet();
        int nClear = 0;
        sal_uInt16 nCount = rNewSet.Count();

        if( const SwFormatFootnote* pItem = rNewSet.GetItemIfSet( RES_TXTATR_FTN, false ) )
        {
            nPos = MapModelToView(&rNode, pItem->GetTextFootnote()->GetStart());
            if (IsIdxInside(nPos, TextFrameIndex(1)))
                Prepare( PrepareHint::FootnoteInvalidation, pAttrSetChangeHint->m_pNew );
            nClear = 0x01;
            --nCount;
        }

        if( const SwFormatField* pItem = rNewSet.GetItemIfSet( RES_TXTATR_FIELD, false ) )
        {
            nPos = MapModelToView(&rNode, pItem->GetTextField()->GetStart());
            if (IsIdxInside(nPos, TextFrameIndex(1)))
            {
                const SwFormatField* pOldItem = pAttrSetChangeHint->m_pOld ?
                    &(pAttrSetChangeHint->m_pOld->GetChgSet()->Get(RES_TXTATR_FIELD)) : nullptr;
                if (SfxPoolItem::areSame( pItem, pOldItem ))
                {
                    InvalidatePage();
                    SetCompletePaint();
                }
                else
                    InvalidateRange_(SwCharRange(nPos, TextFrameIndex(1)));
            }
            nClear |= 0x02;
            --nCount;
        }
        bool bLineSpace = SfxItemState::SET == rNewSet.GetItemState(
                                        RES_PARATR_LINESPACING, false ),
                 bRegister  = SfxItemState::SET == rNewSet.GetItemState(
                                        RES_PARATR_REGISTER, false );
        if ( bLineSpace || bRegister )
        {
            if (!m_pMergedPara || m_pMergedPara->pParaPropsNode == &rModify)
            {
                Prepare( bRegister ? PrepareHint::Register : PrepareHint::AdjustSizeWithoutFormatting );
                CalcLineSpace();
                InvalidateSize();
                InvalidatePrt_();

                // i#11859
                //  (1) Also invalidate next frame on next page/column.
                //  (2) Skip empty sections and hidden paragraphs
                //  Thus, use method <InvalidateNextPrtArea()>
                InvalidateNextPrtArea();

                SetCompletePaint();
            }
            nClear |= 0x04;
            if ( bLineSpace )
            {
                --nCount;
                if ((!m_pMergedPara || m_pMergedPara->pParaPropsNode == &rModify)
                    && IsInSct() && !GetPrev())
                {
                    SwSectionFrame *pSect = FindSctFrame();
                    if( pSect->ContainsAny() == this )
                        pSect->InvalidatePrt();
                }
            }
            if ( bRegister )
                --nCount;
        }
        if ( SfxItemState::SET == rNewSet.GetItemState( RES_PARATR_SPLIT,
                                                   false ))
        {
            if (!m_pMergedPara || m_pMergedPara->pParaPropsNode == &rModify)
            {
                if (GetPrev())
                    CheckKeep();
                Prepare();
                InvalidateSize();
            }
            nClear |= 0x08;
            --nCount;
        }

        if( SfxItemState::SET == rNewSet.GetItemState( RES_BACKGROUND, false)
            && (!m_pMergedPara || m_pMergedPara->pParaPropsNode == &rModify)
            && !IsFollow() && GetDrawObjs() )
        {
            SwSortedObjs *pObjs = GetDrawObjs();
            for ( size_t i = 0; GetDrawObjs() && i < pObjs->size(); ++i )
            {
                SwAnchoredObject* pAnchoredObj = (*pObjs)[i];
                if ( auto pFly = pAnchoredObj->DynCastFlyFrame() )
                {
                    if( !pFly->IsFlyInContentFrame() )
                    {
                        const SvxBrushItem &rBack =
                            pFly->GetAttrSet()->GetBackground();
                        //     #GetTransChg#
                        //     following condition determines, if the fly frame
                        //     "inherites" the background color of text frame.
                        //     This is the case, if fly frame background
                        //     color is "no fill"/"auto fill" and if the fly frame
                        //     has no background graphic.
                        //     Thus, check complete fly frame background
                        //     color and *not* only its transparency value
                        if ( (rBack.GetColor() == COL_TRANSPARENT)  &&
                            rBack.GetGraphicPos() == GPOS_NONE )
                        {
                            pFly->SetCompletePaint();
                            pFly->InvalidatePage();
                        }
                    }
                }
            }
        }

        if ( SfxItemState::SET ==
             rNewSet.GetItemState( RES_TXTATR_CHARFMT, false ) )
        {
            lcl_SetWrong( *this, rNode, 0, COMPLETE_STRING, false );
            lcl_SetScriptInval( *this, TextFrameIndex(0) );
        }
        else if ( SfxItemState::SET ==
                  rNewSet.GetItemState( RES_CHRATR_LANGUAGE, false ) ||
                  SfxItemState::SET ==
                  rNewSet.GetItemState( RES_CHRATR_CJK_LANGUAGE, false ) ||
                  SfxItemState::SET ==
                  rNewSet.GetItemState( RES_CHRATR_CTL_LANGUAGE, false ) )
            lcl_SetWrong( *this, rNode, 0, COMPLETE_STRING, false );
        else if ( SfxItemState::SET ==
                  rNewSet.GetItemState( RES_CHRATR_FONT, false ) ||
                  SfxItemState::SET ==
                  rNewSet.GetItemState( RES_CHRATR_CJK_FONT, false ) ||
                  SfxItemState::SET ==
                  rNewSet.GetItemState( RES_CHRATR_CTL_FONT, false ) )
            lcl_SetScriptInval( *this, TextFrameIndex(0) );
        else if ( SfxItemState::SET ==
                  rNewSet.GetItemState( RES_FRAMEDIR, false )
            && (!m_pMergedPara || m_pMergedPara->pParaPropsNode == &rModify))
        {
            SetDerivedR2L( false );
            CheckDirChange();
            // Force complete paint due to existing indents.
            SetCompletePaint();
        }

        if( nCount )
        {
            if( getRootFrame()->GetCurrShell() )
            {
                Prepare();
                InvalidatePrt_();
            }

            if (nClear || (m_pMergedPara &&
                    (m_pMergedPara->pParaPropsNode != &rModify ||
                     m_pMergedPara->pFirstNode != &rModify)))
            {
                assert(pAttrSetChangeHint->m_pOld);
                SwAttrSetChg aOldSet( *pAttrSetChangeHint->m_pOld );
                SwAttrSetChg aNewSet( *pAttrSetChangeHint->m_pNew );

                if (m_pMergedPara && m_pMergedPara->pParaPropsNode != &rModify)
                {
                    for (sal_uInt16 i = RES_PARATR_BEGIN; i != RES_FRMATR_END; ++i)
                    {
                        if (i != RES_BREAK && i != RES_PAGEDESC)
                        {
                            aOldSet.ClearItem(i);
                            aNewSet.ClearItem(i);
                        }
                    }
                    for (sal_uInt16 i = XATTR_FILL_FIRST; i <= XATTR_FILL_LAST; ++i)
                    {
                        aOldSet.ClearItem(i);
                        aNewSet.ClearItem(i);
                    }
                }
                if (m_pMergedPara && m_pMergedPara->pFirstNode != &rModify)
                {
                    aOldSet.ClearItem(RES_BREAK);
                    aNewSet.ClearItem(RES_BREAK);
                    aOldSet.ClearItem(RES_PAGEDESC);
                    aNewSet.ClearItem(RES_PAGEDESC);
                }

                if( 0x01 & nClear )
                {
                    aOldSet.ClearItem( RES_TXTATR_FTN );
                    aNewSet.ClearItem( RES_TXTATR_FTN );
                }
                if( 0x02 & nClear )
                {
                    aOldSet.ClearItem( RES_TXTATR_FIELD );
                    aNewSet.ClearItem( RES_TXTATR_FIELD );
                }
                if ( 0x04 & nClear )
                {
                    if ( bLineSpace )
                    {
                        aOldSet.ClearItem( RES_PARATR_LINESPACING );
                        aNewSet.ClearItem( RES_PARATR_LINESPACING );
                    }
                    if ( bRegister )
                    {
                        aOldSet.ClearItem( RES_PARATR_REGISTER );
                        aNewSet.ClearItem( RES_PARATR_REGISTER );
                    }
                }
                if ( 0x08 & nClear )
                {
                    aOldSet.ClearItem( RES_PARATR_SPLIT );
                    aNewSet.ClearItem( RES_PARATR_SPLIT );
                }
                if (aOldSet.Count() || aNewSet.Count())
                {
                    SwContentFrame::SwClientNotify(rModify, sw::AttrSetChangeHint(&aOldSet, &aNewSet));
                }
            }
            else
                SwContentFrame::SwClientNotify(rModify, rHint);
        }

#if !ENABLE_WASM_STRIP_ACCESSIBILITY
        if (isA11yRelevantAttribute(nWhich))
        {
            SwViewShell* pViewSh = getRootFrame() ? getRootFrame()->GetCurrShell() : nullptr;
            if ( pViewSh  )
            {
                pViewSh->InvalidateAccessibleParaAttrs( *this );
            }
        }
#endif
    }
    else if (rHint.GetId() == SfxHintId::SwObjectDying)
        ; // do nothing
    else if (pUpdateAttrHint)
    {
        const SwUpdateAttr* pNewUpdate = pUpdateAttrHint->m_pNew;

        sal_Int32 const nNPos = pNewUpdate->getStart();
        sal_Int32 const nNLen = pNewUpdate->getEnd() - nNPos;
        nPos = MapModelToView(&rNode, nNPos);
        nLen = MapModelToView(&rNode, nNPos + nNLen) - nPos;
        if( IsIdxInside( nPos, nLen ) )
        {
            // We need to reformat anyways, even if the invalidated
            // range is empty.
            // E.g.: empty line, set 14 pt!

            // FootnoteNumbers need to be formatted
            if( !nLen )
                nLen = TextFrameIndex(1);

            InvalidateRange_( SwCharRange( nPos, nLen) );
            const sal_uInt16 nTmp = pNewUpdate->getWhichAttr();

            if( ! nTmp || RES_TXTATR_CHARFMT == nTmp || RES_TXTATR_INETFMT == nTmp || RES_TXTATR_AUTOFMT == nTmp ||
                RES_UPDATEATTR_FMT_CHG == nTmp || RES_UPDATEATTR_ATTRSET_CHG == nTmp )
            {
                lcl_SetWrong( *this, rNode, nNPos, nNPos + nNLen, false );
                lcl_SetScriptInval( *this, nPos );
            }
        }

#if !ENABLE_WASM_STRIP_ACCESSIBILITY
        if( isA11yRelevantAttribute( pNewUpdate->getWhichAttr() ) &&
            hasA11yRelevantAttribute( pNewUpdate->getFmtAttrs() ) )
        {
            SwViewShell* pViewSh = getRootFrame() ? getRootFrame()->GetCurrShell() : nullptr;
            if ( pViewSh  )
            {
                pViewSh->InvalidateAccessibleParaAttrs( *this );
            }
        }
#endif
    }
    else switch (nWhich)
    {
        case RES_LINENUMBER:
        {
            assert(false); // should have been forwarded to SwContentFrame
            InvalidateLineNum();
        }
        break;

        case RES_PARATR_LINESPACING:
            {
                CalcLineSpace();
                InvalidateSize();
                InvalidatePrt_();
                if( IsInSct() && !GetPrev() )
                {
                    SwSectionFrame *pSect = FindSctFrame();
                    if( pSect->ContainsAny() == this )
                        pSect->InvalidatePrt();
                }

                // i#11859
                //  (1) Also invalidate next frame on next page/column.
                //  (2) Skip empty sections and hidden paragraphs
                //  Thus, use method <InvalidateNextPrtArea()>
                InvalidateNextPrtArea();

                SetCompletePaint();
            }
            break;

        case RES_TXTATR_FIELD:
        case RES_TXTATR_ANNOTATION:
            {
                sal_Int32 const nNPos = static_cast<const SwFormatField*>(pNew)->GetTextField()->GetStart();
                nPos = MapModelToView(&rNode, nNPos);
                if (IsIdxInside(nPos, TextFrameIndex(1)))
                {
                    if (SfxPoolItem::areSame( pNew, pOld ))
                    {
                        // only repaint
                        // opt: invalidate window?
                        InvalidatePage();
                        SetCompletePaint();
                    }
                    else
                        InvalidateRange_(SwCharRange(nPos, TextFrameIndex(1)));
                }
                bSetFieldsDirty = true;
                // ST2
                if ( SwSmartTagMgr::Get().IsSmartTagsEnabled() )
                    lcl_SetWrong( *this, rNode, nNPos, nNPos + 1, false );
            }
            break;

        case RES_TXTATR_FTN :
        {
            if (!IsInFootnote())
            {   // the hint may be sent from the anchor node, or from a
                // node in the footnote; the anchor index is only valid in the
                // anchor node!
                assert(rNode == static_cast<const SwFormatFootnote*>(pNew)->GetTextFootnote()->GetTextNode());
                nPos = MapModelToView(&rNode,
                    static_cast<const SwFormatFootnote*>(pNew)->GetTextFootnote()->GetStart());
            }
#ifdef _MSC_VER
            else nPos = TextFrameIndex(42); // shut up MSVC 2017 spurious warning C4701
#endif
            if (IsInFootnote() || IsIdxInside(nPos, TextFrameIndex(1)))
                Prepare( PrepareHint::FootnoteInvalidation, static_cast<const SwFormatFootnote*>(pNew)->GetTextFootnote() );
            break;
        }

        case RES_PARATR_SPLIT:
            if ( GetPrev() )
                CheckKeep();
            Prepare();
            bSetFieldsDirty = true;
            break;
        case RES_FRAMEDIR :
            assert(false); // should have been forwarded to SwContentFrame
            SetDerivedR2L( false );
            CheckDirChange();
            break;
        default:
        {
            Prepare();
            InvalidatePrt_();
            if ( !nWhich )
            {
                // is called by e. g. HiddenPara with 0
                SwFrame *pNxt = FindNext();
                if ( nullptr != pNxt )
                    pNxt->InvalidatePrt();
            }
        }
    } // switch

    if( bSetFieldsDirty )
        GetDoc().getIDocumentFieldsAccess().SetFieldsDirty( true, &rNode, SwNodeOffset(1) );

    if ( bRecalcFootnoteFlag )
        CalcFootnoteFlag();
}

void SwTextFrame::PrepWidows( const sal_uInt16 nNeed, bool bNotify )
{
    OSL_ENSURE(GetFollow() && nNeed, "+SwTextFrame::Prepare: lost all friends");

    SwParaPortion *pPara = GetPara();
    if ( !pPara )
        return;
    pPara->SetPrepWidows();

    sal_uInt16 nHave = nNeed;

    // We yield a few lines and shrink in CalcPreps()
    SwSwapIfNotSwapped swap( this );

    SwTextSizeInfo aInf( this );
    SwTextMargin aLine( this, &aInf );
    aLine.Bottom();
    TextFrameIndex nTmpLen = aLine.GetCurr()->GetLen();
    while( nHave && aLine.PrevLine() )
    {
        if( nTmpLen )
            --nHave;
        nTmpLen = aLine.GetCurr()->GetLen();
    }

    // If it's certain that we can yield lines, the Master needs
    // to check the widow rule
    if( !nHave )
    {
        bool bSplit = true;
        if( !IsFollow() ) // only a master decides about orphans
        {
            const WidowsAndOrphans aWidOrp( this );
            bSplit = ( aLine.GetLineNr() >= aWidOrp.GetOrphansLines() &&
                       aLine.GetLineNr() >= aLine.GetDropLines() );
        }

        if( bSplit )
        {
            GetFollow()->SetOffset( aLine.GetEnd() );
            aLine.TruncLines( true );
            if( pPara->IsFollowField() )
                GetFollow()->SetFieldFollow( true );
        }
    }
    if ( bNotify )
    {
        InvalidateSize_();
        InvalidatePage();
    }
}

static bool lcl_ErgoVadis(SwTextFrame* pFrame, TextFrameIndex & rPos, const PrepareHint ePrep)
{
    const SwFootnoteInfo &rFootnoteInfo = pFrame->GetDoc().GetFootnoteInfo();
    if( ePrep == PrepareHint::ErgoSum )
    {
        if( rFootnoteInfo.m_aErgoSum.isEmpty() )
            return false;
        rPos = pFrame->GetOffset();
    }
    else
    {
        if( rFootnoteInfo.m_aQuoVadis.isEmpty() )
            return false;
        if( pFrame->HasFollow() )
            rPos = pFrame->GetFollow()->GetOffset();
        else
            rPos = TextFrameIndex(pFrame->GetText().getLength());
        if( rPos )
            --rPos; // our last character
    }
    return true;
}

// Silence over-eager warning emitted at least by GCC 5.3.1
#if defined __GNUC__ && !defined __clang__
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wstrict-overflow"
#endif
bool SwTextFrame::Prepare( const PrepareHint ePrep, const void* pVoid,
                        bool bNotify )
{
    bool bParaPossiblyInvalid = false;

    SwFrameSwapper aSwapper( this, false );

    if ( IsEmpty() )
    {
        switch ( ePrep )
        {
            case PrepareHint::BossChanged:
                SetInvalidVert( true ); // Test
                [[fallthrough]];
            case PrepareHint::WidowsOrphans:
            case PrepareHint::Widows:
            case PrepareHint::FootnoteInvalidationGone :    return bParaPossiblyInvalid;

            case PrepareHint::FramePositionChanged :
            {
                // We also need an InvalidateSize for Areas (with and without columns),
                // so that we format and bUndersized is set (if needed)
                if( IsInFly() || IsInSct() )
                {
                    SwTwips nTmpBottom = GetUpper()->getFrameArea().Top() +
                        GetUpper()->getFramePrintArea().Bottom();
                    if( nTmpBottom < getFrameArea().Bottom() )
                        break;
                }
                // Are there any free-flying frames on this page?
                SwTextFly aTextFly( this );
                if( aTextFly.IsOn() )
                {
                    // Does any free-flying frame overlap?
                    if ( aTextFly.Relax() || IsUndersized() )
                        break;
                }
                if (GetTextNodeForParaProps()->GetSwAttrSet().GetRegister().GetValue())
                    break;

                SwTextGridItem const*const pGrid(GetGridItem(FindPageFrame()));
                if (pGrid && GetTextNodeForParaProps()->GetSwAttrSet().GetParaGrid().GetValue())
                    break;

                // i#28701 - consider anchored objects
                if ( GetDrawObjs() )
                    break;

                return bParaPossiblyInvalid;
            }
            default:
                break;
        }
    }

    // Split fly anchors are technically empty (have no SwParaPortion), but otherwise behave like
    // other split text frames, which are non-empty.
    bool bSplitFlyAnchor = GetOffset() == TextFrameIndex(0) && HasFollow()
                           && GetFollow()->GetOffset() == TextFrameIndex(0);

    if( !HasPara() && !bSplitFlyAnchor && PrepareHint::MustFit != ePrep )
    {
        OSL_ENSURE( !IsLocked(), "SwTextFrame::Prepare: three of a perfect pair" );
        // check while ignoring frame width (testParagraphMarkInCell)
        // because it's called from InvalidateAllContent()
        if (!IsHiddenNowImpl())
        {
            SetInvalidVert( true ); // Test
            if ( bNotify )
                InvalidateSize();
            else
                InvalidateSize_();
        }
        return bParaPossiblyInvalid;
    }

    // Get object from cache while locking
    SwTextLineAccess aAccess( this );
    SwParaPortion *pPara = aAccess.GetPara();

    switch( ePrep )
    {
        case PrepareHint::FootnoteMove :
            {
                SwFrameAreaDefinition::FrameAreaWriteAccess aFrm(*this);
                aFrm.Height(0);
            }

            {
                SwFrameAreaDefinition::FramePrintAreaWriteAccess aPrt(*this);
                aPrt.Height(0);
            }

            InvalidatePrt_();
            InvalidateSize_();
            [[fallthrough]];
        case PrepareHint::AdjustSizeWithoutFormatting :
            pPara->SetPrepAdjust();
            if( IsFootnoteNumFrame() != pPara->IsFootnoteNum() ||
                IsUndersized() )
            {
                InvalidateRange(SwCharRange(TextFrameIndex(0), TextFrameIndex(1)), 1);
                if( GetOffset() && !IsFollow() )
                    SetOffset_(TextFrameIndex(0));
            }
            break;
        case PrepareHint::MustFit :
            pPara->SetPrepMustFit(true);
            [[fallthrough]];
        case PrepareHint::WidowsOrphans :
            pPara->SetPrepAdjust();
            break;
        case PrepareHint::Widows :
            // MustFit is stronger than anything else
            if( pPara->IsPrepMustFit() )
                return bParaPossiblyInvalid;
            // see comment in WidowsAndOrphans::FindOrphans and CalcPreps()
            PrepWidows( *static_cast<const sal_uInt16 *>(pVoid), bNotify );
            break;

        case PrepareHint::FootnoteInvalidation :
        {
            SwTextFootnote const *pFootnote = static_cast<SwTextFootnote const *>(pVoid);
            if( IsInFootnote() )
            {
                // Am I the first TextFrame of a footnote?
                if( !GetPrev() )
                    // So we're a TextFrame of the footnote, which has
                    // to display the footnote number or the ErgoSum text
                    InvalidateRange(SwCharRange(TextFrameIndex(0), TextFrameIndex(1)), 1);

                if( !GetNext() )
                {
                    // We're the last Footnote; we need to update the
                    // QuoVadis texts now
                    const SwFootnoteInfo &rFootnoteInfo = GetDoc().GetFootnoteInfo();
                    if( !pPara->UpdateQuoVadis( rFootnoteInfo.m_aQuoVadis ) )
                    {
                        TextFrameIndex nPos = pPara->GetParLen();
                        if( nPos )
                            --nPos;
                        InvalidateRange( SwCharRange(nPos, TextFrameIndex(1)), 1);
                    }
                }
            }
            else
            {
                // We are the TextFrame _with_ the footnote
                TextFrameIndex const nPos = MapModelToView(
                        &pFootnote->GetTextNode(), pFootnote->GetStart());
                InvalidateRange(SwCharRange(nPos, TextFrameIndex(1)), 1);
            }
            break;
        }
        case PrepareHint::BossChanged :
        {
            // Test
            {
                SetInvalidVert( false );
                bool bOld = IsVertical();
                SetInvalidVert( true );
                if( bOld != IsVertical() )
                    InvalidateRange(SwCharRange(GetOffset(), TextFrameIndex(COMPLETE_STRING)));
            }

            if( HasFollow() )
            {
                TextFrameIndex nNxtOfst = GetFollow()->GetOffset();
                if( nNxtOfst )
                    --nNxtOfst;
                InvalidateRange(SwCharRange( nNxtOfst, TextFrameIndex(1)), 1);
            }
            if( IsInFootnote() )
            {
                TextFrameIndex nPos;
                if( lcl_ErgoVadis( this, nPos, PrepareHint::QuoVadis ) )
                    InvalidateRange( SwCharRange( nPos, TextFrameIndex(1)) );
                if( lcl_ErgoVadis( this, nPos, PrepareHint::ErgoSum ) )
                    InvalidateRange( SwCharRange( nPos, TextFrameIndex(1)) );
            }
            // If we have a page number field, we must invalidate those spots
            SwTextNode const* pNode(nullptr);
            sw::MergedAttrIter iter(*this);
            TextFrameIndex const nEnd = GetFollow()
                    ? GetFollow()->GetOffset() : TextFrameIndex(COMPLETE_STRING);
            for (SwTextAttr const* pHt = iter.NextAttr(&pNode); pHt; pHt = iter.NextAttr(&pNode))
            {
                TextFrameIndex const nStart(MapModelToView(pNode, pHt->GetStart()));
                if (nStart >= GetOffset())
                {
                    if (nStart >= nEnd)
                        break;

                // If we're flowing back and own a Footnote, the Footnote also flows
                // with us. So that it doesn't obstruct us, we send ourselves
                // an ADJUST_FRM.
                // pVoid != 0 means MoveBwd()
                    const sal_uInt16 nWhich = pHt->Which();
                    if (RES_TXTATR_FIELD == nWhich ||
                        (HasFootnote() && pVoid && RES_TXTATR_FTN == nWhich))
                        InvalidateRange(SwCharRange(nStart, TextFrameIndex(1)), 1);
                }
            }
            // A new boss, a new chance for growing
            if( IsUndersized() )
            {
                InvalidateSize_();
                InvalidateRange(SwCharRange(GetOffset(), TextFrameIndex(1)), 1);
            }
            break;
        }

        case PrepareHint::FramePositionChanged :
        {
            if ( isFramePrintAreaValid() )
            {
                SwTextGridItem const*const pGrid(GetGridItem(FindPageFrame()));
                if (pGrid && GetTextNodeForParaProps()->GetSwAttrSet().GetParaGrid().GetValue())
                    InvalidatePrt();
            }

            // If we don't overlap with anybody:
            // did any free-flying frame overlapped _before_ the position change?
            bool bFormat = pPara->HasFly();
            if( !bFormat )
            {
                if( IsInFly() )
                {
                    SwTwips nTmpBottom = GetUpper()->getFrameArea().Top() +
                        GetUpper()->getFramePrintArea().Bottom();
                    if( nTmpBottom < getFrameArea().Bottom() )
                        bFormat = true;
                }
                if( !bFormat )
                {
                    if ( GetDrawObjs() )
                    {
                        const size_t nCnt = GetDrawObjs()->size();
                        for ( size_t i = 0; i < nCnt; ++i )
                        {
                            SwAnchoredObject* pAnchoredObj = (*GetDrawObjs())[i];
                            // i#28701 - consider all
                            // to-character anchored objects
                            if ( pAnchoredObj->GetFrameFormat()->GetAnchor().GetAnchorId()
                                    == RndStdIds::FLY_AT_CHAR )
                            {
                                bFormat = true;
                                break;
                            }
                        }
                    }
                    if( !bFormat )
                    {
                        // Are there any free-flying frames on this page?
                        SwTextFly aTextFly( this );
                        if( aTextFly.IsOn() )
                        {
                            // Does any free-flying frame overlap?
                            const bool bRelaxed = aTextFly.Relax();
                            bFormat = bRelaxed || IsUndersized();
                            if (bRelaxed)
                            {
                                // It's possible that pPara was deleted above; retrieve it again
                                pPara = aAccess.GetPara();
                            }
                        }
                    }
                }
            }

            if( bFormat )
            {
                if( !IsLocked() )
                {
                    if( pPara->GetRepaint().HasArea() )
                        SetCompletePaint();
                    Init();
                    pPara = nullptr;
                    InvalidateSize_();
                }
            }
            else
            {
                if (GetTextNodeForParaProps()->GetSwAttrSet().GetRegister().GetValue())
                    bParaPossiblyInvalid = Prepare( PrepareHint::Register, nullptr, bNotify );
                // The Frames need to be readjusted, which caused by changes
                // in position
                else if( HasFootnote() )
                {
                    bParaPossiblyInvalid = Prepare( PrepareHint::AdjustSizeWithoutFormatting, nullptr, bNotify );
                    InvalidateSize_();
                }
                else
                    return bParaPossiblyInvalid; // So that there's no SetPrep()

                if (bParaPossiblyInvalid)
                {
                    // It's possible that pPara was deleted above; retrieve it again
                    pPara = aAccess.GetPara();
                }

            }
            break;
        }
        case PrepareHint::Register:
            if (GetTextNodeForParaProps()->GetSwAttrSet().GetRegister().GetValue())
            {
                pPara->SetPrepAdjust();
                CalcLineSpace();

                // It's possible that pPara was deleted above; retrieve it again
                bParaPossiblyInvalid = true;
                pPara = aAccess.GetPara();

                InvalidateSize();
                InvalidatePrt_();
                SwFrame* pNxt = GetIndNext();
                if ( nullptr != pNxt )
                {
                    pNxt->InvalidatePrt_();
                    if ( pNxt->IsLayoutFrame() )
                        pNxt->InvalidatePage();
                }
                SetCompletePaint();
            }
            break;
        case PrepareHint::FootnoteInvalidationGone :
            {
                // If a Follow is calling us, because a footnote is being deleted, our last
                // line has to be formatted, so that the first line of the Follow can flow up.
                // Which had flowed to the next page to be together with the footnote (this is
                // especially true for areas with columns)
                OSL_ENSURE( GetFollow(), "PrepareHint::FootnoteInvalidationGone may only be called by Follow" );
                TextFrameIndex nPos = GetFollow()->GetOffset();
                if( IsFollow() && GetOffset() == nPos )       // If we don't have a mass of text, we call our
                    FindMaster()->Prepare( PrepareHint::FootnoteInvalidationGone ); // Master's Prepare
                if( nPos )
                    --nPos; // The char preceding our Follow
                InvalidateRange(SwCharRange(nPos, TextFrameIndex(1)));
                return bParaPossiblyInvalid;
            }
        case PrepareHint::ErgoSum:
        case PrepareHint::QuoVadis:
            {
                TextFrameIndex nPos;
                if( lcl_ErgoVadis( this, nPos, ePrep ) )
                    InvalidateRange(SwCharRange(nPos, TextFrameIndex(1)));
            }
            break;
        case PrepareHint::FlyFrameAttributesChanged:
        {
            if( pVoid )
            {
                TextFrameIndex const nWhere = CalcFlyPos( static_cast<SwFrameFormat const *>(pVoid) );
                OSL_ENSURE( TextFrameIndex(COMPLETE_STRING) != nWhere, "Prepare: Why me?" );
                InvalidateRange(SwCharRange(nWhere, TextFrameIndex(1)));
                return bParaPossiblyInvalid;
            }
            [[fallthrough]]; // else: continue with default case block
        }
        case PrepareHint::Clear:
        default:
        {
            if( IsLocked() )
            {
                if( PrepareHint::FlyFrameArrive == ePrep || PrepareHint::FlyFrameLeave == ePrep )
                {
                    TextFrameIndex const nLen = (GetFollow()
                                ? GetFollow()->GetOffset()
                                : TextFrameIndex(COMPLETE_STRING))
                            - GetOffset();
                    InvalidateRange( SwCharRange( GetOffset(), nLen ) );
                }
            }
            else
            {
                if( pPara->GetRepaint().HasArea() )
                    SetCompletePaint();
                Init();
                pPara = nullptr;
                if( GetOffset() && !IsFollow() )
                    SetOffset_( TextFrameIndex(0) );
                if ( bNotify )
                    InvalidateSize();
                else
                    InvalidateSize_();
            }
            return bParaPossiblyInvalid; // no SetPrep() happened
        }
    }
    if( pPara )
    {
        pPara->SetPrep();
    }

    return bParaPossiblyInvalid;
}
#if defined __GNUC__ && !defined __clang__
# pragma GCC diagnostic pop
#endif

/**
 * Small Helper class:
 * Prepares a test format.
 * The frame is changed in size and position, its SwParaPortion is moved aside
 * and a new one is created.
 * To achieve this, run formatting with bTestFormat flag set.
 * In the destructor the TextFrame is reset to its original state.
 */
class SwTestFormat
{
    SwTextFrame *pFrame;
    SwParaPortion *pOldPara;
    SwRect aOldFrame, aOldPrt;
public:
    SwTestFormat( SwTextFrame* pTextFrame, const SwFrame* pPrv, SwTwips nMaxHeight );
    ~SwTestFormat();
};

SwTestFormat::SwTestFormat( SwTextFrame* pTextFrame, const SwFrame* pPre, SwTwips nMaxHeight )
    : pFrame( pTextFrame )
{
    aOldFrame = pFrame->getFrameArea();
    aOldPrt = pFrame->getFramePrintArea();

    SwRectFnSet aRectFnSet(pFrame);
    SwTwips nLower = aRectFnSet.GetBottomMargin(*pFrame);

    {
        // indeed, here the GetUpper()->getFramePrintArea() gets copied and manipulated
        SwFrameAreaDefinition::FrameAreaWriteAccess aFrm(*pFrame);
        aFrm.setSwRect(pFrame->GetUpper()->getFramePrintArea());
        aFrm += pFrame->GetUpper()->getFrameArea().Pos();
        aRectFnSet.SetHeight( aFrm, nMaxHeight );

        if( pFrame->GetPrev() )
        {
            aRectFnSet.SetPosY(
                aFrm,
                aRectFnSet.GetBottom(pFrame->GetPrev()->getFrameArea()) - ( aRectFnSet.IsVert() ? nMaxHeight + 1 : 0 ) );
        }
    }

    SwBorderAttrAccess aAccess( SwFrame::GetCache(), pFrame );
    const SwBorderAttrs &rAttrs = *aAccess.Get();

    {
        SwFrameAreaDefinition::FramePrintAreaWriteAccess aPrt(*pFrame);
        aRectFnSet.SetPosX(aPrt, rAttrs.CalcLeft( pFrame ) );
    }

    if( pPre )
    {
        SwTwips nUpper = pFrame->CalcUpperSpace( &rAttrs, pPre );
        SwFrameAreaDefinition::FramePrintAreaWriteAccess aPrt(*pFrame);
        aRectFnSet.SetPosY(aPrt, nUpper );
    }

    {
        SwFrameAreaDefinition::FramePrintAreaWriteAccess aPrt(*pFrame);
        aRectFnSet.SetHeight( aPrt, std::max( tools::Long(0) , aRectFnSet.GetHeight(pFrame->getFrameArea()) - aRectFnSet.GetTop(aPrt) - nLower ) );
        aRectFnSet.SetWidth( aPrt, aRectFnSet.GetWidth(pFrame->getFrameArea()) - ( rAttrs.CalcLeft( pFrame ) + rAttrs.CalcRight( pFrame ) ) );
    }

    pOldPara = pFrame->HasPara() ? pFrame->GetPara() : nullptr;
    pFrame->SetPara( new SwParaPortion(), false );
    OSL_ENSURE( ! pFrame->IsSwapped(), "A frame is swapped before Format_" );

    if ( pFrame->IsVertical() )
        pFrame->SwapWidthAndHeight();

    SwTextFormatInfo aInf( pFrame->getRootFrame()->GetCurrShell()->GetOut(), pFrame, false, true, true );
    SwTextFormatter  aLine( pFrame, &aInf );

    pFrame->Format_( aLine, aInf );

    if ( pFrame->IsVertical() )
        pFrame->SwapWidthAndHeight();

    OSL_ENSURE( ! pFrame->IsSwapped(), "A frame is swapped after Format_" );
}

SwTestFormat::~SwTestFormat()
{
    {
        SwFrameAreaDefinition::FrameAreaWriteAccess aFrm(*pFrame);
        aFrm.setSwRect(aOldFrame);
    }

    {
        SwFrameAreaDefinition::FramePrintAreaWriteAccess aPrt(*pFrame);
        aPrt.setSwRect(aOldPrt);
    }

    pFrame->SetPara( pOldPara );
}

bool SwTextFrame::TestFormat( const SwFrame* pPrv, SwTwips &rMaxHeight, bool &bSplit )
{
    PROTOCOL_ENTER( this, PROT::TestFormat, DbgAction::NONE, nullptr )

    if( IsLocked() && GetUpper()->getFramePrintArea().Width() <= 0 )
        return false;

    SwTestFormat aSave( this, pPrv, rMaxHeight );

    return SwTextFrame::WouldFit(rMaxHeight, bSplit, true, false);
}

/**
 * We should not and don't need to reformat.
 * We assume that we already formatted and that the formatting
 * data is still current.
 *
 * We also assume that the frame width of the Master and Follow
 * are the same. That's why we're not calling FindBreak() for
 * FindOrphans().
 * The required height is coming from nMaxHeight.
 *
 * @returns true if I can split
 */
bool SwTextFrame::WouldFit(SwTwips &rMaxHeight, bool &bSplit, bool bTst, bool bMoveBwd)
{
    OSL_ENSURE( ! IsVertical() || ! IsSwapped(),
            "SwTextFrame::WouldFit with swapped frame" );
    SwRectFnSet aRectFnSet(this);

    if( IsLocked() )
        return false;

    // it can happen that the IdleCollector removed the cached information
    if( !IsEmpty() )
        GetFormatted();

    // i#27801 - correction: 'short cut' for empty paragraph
    // can *not* be applied, if test format is in progress. The test format doesn't
    // adjust the frame and the printing area - see method <SwTextFrame::Format_(..)>,
    // which is called in <SwTextFrame::TestFormat(..)>
    if ( IsEmpty() && !bTst )
    {
        bSplit = false;
        SwTwips nHeight = aRectFnSet.IsVert() ? getFramePrintArea().SSize().Width() : getFramePrintArea().SSize().Height();
        if( rMaxHeight < nHeight )
            return false;
        else
        {
            rMaxHeight -= nHeight;
            return true;
        }
    }

    // GetPara can still be 0 in edge cases
    // We return true in order to be reformatted on the new Page
    OSL_ENSURE( HasPara() || IsHiddenNow(), "WouldFit: GetFormatted() and then !HasPara()" );
    if( !HasPara() || ( !aRectFnSet.GetHeight(getFrameArea()) && IsHiddenNow() ) )
        return true;

    // Because the Orphan flag only exists for a short moment, we also check
    // whether the Framesize is set to very huge by CalcPreps, in order to
    // force a MoveFwd
    if (IsWidow() || (aRectFnSet.IsVert()
                        ? (0 == getFrameArea().Left())
                        : (sw::WIDOW_MAGIC - 20000 < getFrameArea().Bottom())))
    {
        SetWidow(false);
        if ( GetFollow() )
        {
            // If we've ended up here due to a Widow request by our Follow, we check
            // whether there's a Follow with a real height at all.
            // Else (e.g. for newly created SctFrames) we ignore the IsWidow() and
            // still check if we can find enough room
            if (((!aRectFnSet.IsVert() && getFrameArea().Bottom() <= sw::WIDOW_MAGIC - 20000) ||
                  (   aRectFnSet.IsVert() && 0 < getFrameArea().Left() ) ) &&
                  ( GetFollow()->IsVertical() ?
                    !GetFollow()->getFrameArea().Width() :
                    !GetFollow()->getFrameArea().Height() ) )
            {
                SwTextFrame* pFoll = GetFollow()->GetFollow();
                while( pFoll &&
                        ( pFoll->IsVertical() ?
                         !pFoll->getFrameArea().Width() :
                         !pFoll->getFrameArea().Height() ) )
                    pFoll = pFoll->GetFollow();
                if( pFoll )
                    return false;
            }
            else
                return false;
        }
    }

    SwSwapIfNotSwapped swap( this );

    SwTextSizeInfo aInf( this );
    SwTextMargin aLine( this, &aInf );

    WidowsAndOrphans aFrameBreak( this, rMaxHeight, bSplit );

    bool bRet = true;

    aLine.Bottom();
    // is breaking necessary?
    bSplit = !aFrameBreak.IsInside( aLine );
    if ( bSplit )
        bRet = !aFrameBreak.IsKeepAlways() && aFrameBreak.WouldFit(aLine, rMaxHeight, bTst, bMoveBwd);
    else
    {
        // we need the total height including the current line
        aLine.Top();
        do
        {
            rMaxHeight -= aLine.GetLineHeight();
        } while ( aLine.Next() );
    }

    return bRet;
}

SwTwips SwTextFrame::GetParHeight() const
{
    OSL_ENSURE( ! IsVertical() || ! IsSwapped(),
            "SwTextFrame::GetParHeight with swapped frame" );

    if( !HasPara() )
    {   // For non-empty paragraphs this is a special case
        // For UnderSized we can simply just ask 1 Twip more
        SwTwips nRet = getFramePrintArea().SSize().Height();
        if( IsUndersized() )
        {
            if( IsEmpty() || GetText().isEmpty() )
                nRet = EmptyHeight();
            else
                ++nRet;
        }
        return nRet;
    }

    // TODO: Refactor and improve code
    const SwLineLayout* pLineLayout = GetPara();
    SwTwips nHeight = pLineLayout ? pLineLayout->GetRealHeight() : 0;

    // Is this paragraph scrolled? Our height until now is at least
    // one line height too low then
    if( GetOffset() && !IsFollow() )
        nHeight *= 2;

    while ( pLineLayout && pLineLayout->GetNext() )
    {
        pLineLayout = pLineLayout->GetNext();
        nHeight = nHeight + pLineLayout->GetRealHeight();
    }

    return nHeight;
}

/**
 * @returns this _always_ in the formatted state!
 */
SwTextFrame* SwTextFrame::GetFormatted( bool bForceQuickFormat )
{
    vcl::RenderContext* pRenderContext = getRootFrame()->GetCurrShell()->GetOut();
    SwSwapIfSwapped swap( this );

    // In case the SwLineLayout was cleared out of the s_pTextCache, recreate it
    // Not for empty paragraphs
    if( !HasPara() && !(isFrameAreaDefinitionValid() && IsEmpty()) )
    {
        // Calc() must be called, because frame position can be wrong
        const bool bFormat = isFrameAreaSizeValid();
        Calc(pRenderContext); // calls Format() if invalid

        // If the flags were valid (hence bFormat=true), Calc did nothing,
        // so Format() must be called manually in order to recreate
        // the SwLineLayout that has been deleted from the
        // SwTextFrame::s_pTextCache (hence !HasPara() above).
        // Optimization with FormatQuick()
        if( bFormat && !FormatQuick( bForceQuickFormat ) )
            Format(getRootFrame()->GetCurrShell()->GetOut());
    }

    return this;
}

SwTwips SwTextFrame::CalcFitToContent()
{
    // i#31490
    // If we are currently locked, we better return with a
    // fairly reasonable value:
    if ( IsLocked() )
        return getFramePrintArea().Width();

    SwParaPortion* pOldPara = GetPara();
    SwParaPortion *pDummy = new SwParaPortion();
    SetPara( pDummy, false );
    const SwPageFrame* pPage = FindPageFrame();

    const Point   aOldFramePos   = getFrameArea().Pos();
    const SwTwips nOldFrameWidth = getFrameArea().Width();
    const SwTwips nOldPrtWidth = getFramePrintArea().Width();
    const SwTwips nPageWidth = GetUpper()->IsVertical() ?
                               pPage->getFramePrintArea().Height() :
                               pPage->getFramePrintArea().Width();

    {
        SwFrameAreaDefinition::FrameAreaWriteAccess aFrm(*this);
        aFrm.Width( nPageWidth );
    }

    {
        SwFrameAreaDefinition::FramePrintAreaWriteAccess aPrt(*this);
        aPrt.Width( nPageWidth );
    }

    // i#25422 objects anchored as character in RTL
    if ( IsRightToLeft() )
    {
        SwFrameAreaDefinition::FrameAreaWriteAccess aFrm(*this);
        aFrm.Pos().AdjustX(nOldFrameWidth - nPageWidth );
    }

    TextFrameLockGuard aLock( this );

    SwTextFormatInfo aInf( getRootFrame()->GetCurrShell()->GetOut(), this, false, true, true );
    aInf.SetIgnoreFly( true );
    SwTextFormatter  aLine( this, &aInf );
    SwHookOut aHook( aInf );

    // i#54031 - assure minimum of MINLAY twips.
    const SwTwips nMax = std::max( SwTwips(MINLAY), aLine.CalcFitToContent_() + 1 );

    {
        SwFrameAreaDefinition::FrameAreaWriteAccess aFrm(*this);
        aFrm.Width( nOldFrameWidth );

        // i#25422 objects anchored as character in RTL
        if ( IsRightToLeft() )
        {
            aFrm.Pos() = aOldFramePos;
        }
    }

    {
        SwFrameAreaDefinition::FramePrintAreaWriteAccess aPrt(*this);
        aPrt.Width( nOldPrtWidth );
    }

    SetPara( pOldPara );

    // tdf#164932 handle numbering list offset
    const SwTextNode* pTextNode( GetTextNodeForParaProps() );
    SwTwips nNumOffset = 0;
    if ( pTextNode->IsNumbered(getRootFrame()) &&
        pTextNode->IsCountedInList() && pTextNode->GetNumRule() )
    {
        sal_uInt16 nListLevel = std::clamp(pTextNode->GetActualListLevel(), 0, MAXLEVEL - 1);
        const SwNumFormat& rNumFormat = pTextNode->GetNumRule()->Get(nListLevel);
        if ( rNumFormat.GetPositionAndSpaceMode() == SvxNumberFormat::LABEL_ALIGNMENT )
        {
            const SwAttrSet& rSet = pTextNode->GetSwAttrSet();
            ::sw::ListLevelIndents const indents(pTextNode->AreListLevelIndentsApplicable());
            SvxTextLeftMarginItem leftMargin(rSet.GetTextLeftMargin());
            if (indents & ::sw::ListLevelIndents::LeftMargin)
                leftMargin.SetTextLeft(SvxIndentValue::twips(rNumFormat.GetAbsLSpace()));
            nNumOffset = leftMargin.ResolveTextLeft(/*metrics*/ {});
        }
    }

    return nMax + nNumOffset;
}

/**
 * Simulate format for a list item paragraph, whose list level attributes
 * are in LABEL_ALIGNMENT mode, in order to determine additional first
 * line offset for the real text formatting due to the value of label
 * adjustment attribute of the list level.
 */
void SwTextFrame::CalcAdditionalFirstLineOffset()
{
    if ( IsLocked() )
        return;

    // reset additional first line offset
    mnAdditionalFirstLineOffset = 0;

    const SwTextNode* pTextNode( GetTextNodeForParaProps() );
    // sw_redlinehide: check that pParaPropsNode is the correct one
    assert(pTextNode->IsNumbered(getRootFrame()) == pTextNode->IsNumbered(nullptr));
    if (!(pTextNode->IsNumbered(getRootFrame()) &&
        pTextNode->IsCountedInList() && pTextNode->GetNumRule()))
        return;

    sal_uInt16 nListLevel = std::clamp(pTextNode->GetActualListLevel(), 0, MAXLEVEL - 1);
    const SwNumFormat& rNumFormat = pTextNode->GetNumRule()->Get(nListLevel);
    if ( rNumFormat.GetPositionAndSpaceMode() != SvxNumberFormat::LABEL_ALIGNMENT )
        return;

    // keep current paragraph portion and apply dummy paragraph portion
    SwParaPortion* pOldPara = GetPara();
    SwParaPortion *pDummy = new SwParaPortion();
    SetPara( pDummy, false );

    // lock paragraph
    TextFrameLockGuard aLock( this );

    // simulate text formatting
    SwTextFormatInfo aInf( getRootFrame()->GetCurrShell()->GetOut(), this, false, true, true );
    aInf.SetIgnoreFly( true );
    SwTextFormatter aLine( this, &aInf );
    SwHookOut aHook( aInf );
    aLine.CalcFitToContent_();

    // determine additional first line offset
    const SwLinePortion* pFirstPortion = aLine.GetCurr()->GetFirstPortion();
    if ( pFirstPortion->InNumberGrp() && !pFirstPortion->IsFootnoteNumPortion() )
    {
        SwTwips nNumberPortionWidth( pFirstPortion->Width() );

        const SwLinePortion* pPortion = pFirstPortion->GetNextPortion();
        while ( pPortion &&
                pPortion->InNumberGrp() && !pPortion->IsFootnoteNumPortion())
        {
            nNumberPortionWidth += pPortion->Width();
            pPortion = pPortion->GetNextPortion();
        }

        if ( ( IsRightToLeft() &&
               rNumFormat.GetNumAdjust() == SvxAdjust::Left ) ||
             ( !IsRightToLeft() &&
               rNumFormat.GetNumAdjust() == SvxAdjust::Right ) )
        {
            mnAdditionalFirstLineOffset = -nNumberPortionWidth;
        }
        else if ( rNumFormat.GetNumAdjust() == SvxAdjust::Center )
        {
            mnAdditionalFirstLineOffset = -(nNumberPortionWidth/2);
        }
    }

    // restore paragraph portion
    SetPara( pOldPara );
}

/**
 * Determine the height of the last line for the calculation of
 * the proportional line spacing
 *
 * Height of last line will be stored in new member
 * mnHeightOfLastLine and can be accessed via method
 * GetHeightOfLastLine()
 *
 * @param _bUseFont force the usage of the former algorithm to
 *                  determine the height of the last line, which
 *                  uses the font
 */
void SwTextFrame::CalcHeightOfLastLine( const bool _bUseFont )
{
    // i#71281
    // Invalidate printing area, if height of last line changes
    const SwTwips nOldHeightOfLastLine( mnHeightOfLastLine );

    // determine output device
    SwViewShell* pVsh = getRootFrame()->GetCurrShell();
    OSL_ENSURE( pVsh, "<SwTextFrame::_GetHeightOfLastLineForPropLineSpacing()> - no SwViewShell" );

    // i#78921
    // There could be no <SwViewShell> instance in the case of loading a binary
    // StarOffice file format containing an embedded Writer document.
    if ( !pVsh )
    {
        return;
    }
    OutputDevice* pOut = pVsh->GetOut();
    const IDocumentSettingAccess *const pIDSA = &GetDoc().getIDocumentSettingAccess();
    if ( !pVsh->GetViewOptions()->getBrowseMode() ||
          pVsh->GetViewOptions()->IsPrtFormat() )
    {
        pOut = GetDoc().getIDocumentDeviceAccess().getReferenceDevice( true );
    }
    OSL_ENSURE( pOut, "<SwTextFrame::_GetHeightOfLastLineForPropLineSpacing()> - no OutputDevice" );

    if ( !pOut )
    {
        return;
    }

    // determine height of last line
    if ( _bUseFont || pIDSA->get(DocumentSettingId::OLD_LINE_SPACING ) )
    {
        // former determination of last line height for proportional line
        // spacing - take height of font set at the paragraph
        // FIXME actually... must the font match across all nodes?
        SwFont aFont( &GetTextNodeForParaProps()->GetSwAttrSet(), pIDSA );

        // we must ensure that the font is restored correctly on the OutputDevice
        // otherwise Last!=Owner could occur
        if ( pLastFont )
        {
            SwFntObj *pOldFont = pLastFont;
            pLastFont = nullptr;
            aFont.SetFntChg( true );
            aFont.ChgPhysFnt( pVsh, *pOut );
            mnHeightOfLastLine = aFont.GetHeight( pVsh, *pOut );
            assert(pLastFont && "coverity[var_deref_model] - pLastFont should be set in SwSubFont::ChgFnt");
            pLastFont->Unlock();
            pLastFont = pOldFont;
            pLastFont->SetDevFont( pVsh, *pOut );
        }
        else
        {
            vcl::Font aOldFont = pOut->GetFont();
            aFont.SetFntChg( true );
            aFont.ChgPhysFnt( pVsh, *pOut );
            mnHeightOfLastLine = aFont.GetHeight( pVsh, *pOut );
            assert(pLastFont && "coverity[var_deref_model] - pLastFont should be set in SwSubFont::ChgFnt");
            pLastFont->Unlock();
            pLastFont = nullptr;
            pOut->SetFont( aOldFont );
        }
    }
    else
    {
        // new determination of last line height - take actually height of last line
        // i#89000
        // assure same results, if paragraph is undersized
        if ( IsUndersized() )
        {
            mnHeightOfLastLine = 0;
        }
        else
        {
            bool bCalcHeightOfLastLine = true;
            if ( ( !HasPara() && IsEmpty( ) ) || GetText().isEmpty() )
            {
                mnHeightOfLastLine = EmptyHeight();
                bCalcHeightOfLastLine = false;
            }

            if ( bCalcHeightOfLastLine )
            {
                OSL_ENSURE( HasPara(),
                        "<SwTextFrame::CalcHeightOfLastLine()> - missing paragraph portions." );
                const SwLineLayout* pLineLayout = GetPara();
                while ( pLineLayout && pLineLayout->GetNext() )
                {
                    // iteration to last line
                    pLineLayout = pLineLayout->GetNext();
                }
                if ( pLineLayout )
                {
                    SwTwips nAscent, nDescent, nDummy1, nDummy2;
                    // i#47162 - suppress consideration of
                    // fly content portions and the line portion.
                    pLineLayout->MaxAscentDescent( nAscent, nDescent,
                                                   nDummy1, nDummy2,
                                                   nullptr, true );
                    // i#71281
                    // Suppress wrong invalidation of printing area, if method is
                    // called recursive.
                    // Thus, member <mnHeightOfLastLine> is only set directly, if
                    // no recursive call is needed.
                    const SwTwips nNewHeightOfLastLine = nAscent + nDescent;
                    // i#47162 - if last line only contains
                    // fly content portions, <mnHeightOfLastLine> is zero.
                    // In this case determine height of last line by the font
                    if ( nNewHeightOfLastLine == 0 )
                    {
                        CalcHeightOfLastLine( true );
                    }
                    else
                    {
                        mnHeightOfLastLine = nNewHeightOfLastLine;
                    }
                }
            }
        }
    }
    // i#71281
    // invalidate printing area, if height of last line changes
    if ( mnHeightOfLastLine != nOldHeightOfLastLine )
    {
        InvalidatePrt();
    }
}

/**
 * Method returns the value of the inter line spacing for a text frame.
 * Such a value exists for proportional line spacings ("1,5 Lines",
 * "Double", "Proportional" and for leading line spacing ("Leading").
 *
 * @param _bNoPropLineSpacing (default = false) control whether the
 *                            value of a proportional line spacing is
 *                            returned or not
 */
tools::Long SwTextFrame::GetLineSpace( const bool _bNoPropLineSpace ) const
{
    tools::Long nRet = 0;

    const SvxLineSpacingItem &rSpace = GetTextNodeForParaProps()->GetSwAttrSet().GetLineSpacing();

    switch( rSpace.GetInterLineSpaceRule() )
    {
        case SvxInterLineSpaceRule::Prop:
        {
            if ( _bNoPropLineSpace )
            {
                break;
            }

            // i#11860 - adjust spacing implementation for object positioning
            // - compatibility to MS Word
            nRet = GetHeightOfLastLine();

            tools::Long nTmp = nRet;
            nTmp *= rSpace.GetPropLineSpace();
            nTmp /= 100;
            nTmp -= nRet;
            if ( nTmp > 0 )
                nRet = nTmp;
            else
                nRet = 0;
        }
            break;
        case SvxInterLineSpaceRule::Fix:
        {
            if ( rSpace.GetInterLineSpace() > 0 )
                nRet = rSpace.GetInterLineSpace();
        }
            break;
        default:
            break;
    }
    return nRet;
}

SwTwips SwTextFrame::FirstLineHeight() const
{
    if ( !HasPara() )
    {
        if( IsEmpty() && isFrameAreaDefinitionValid() )
            return IsVertical() ? getFramePrintArea().Width() : getFramePrintArea().Height();
        return std::numeric_limits<SwTwips>::max();
    }
    const SwParaPortion *pPara = GetPara();
    if ( !pPara )
        return std::numeric_limits<SwTwips>::max();

    // tdf#146500 Lines with only fly overlap cannot be "moved", so the idea
    // here is to continue until there's some text.
    // FIXME ideally we want to count a fly to the line in which it is anchored
    // - it may even be anchored in some other paragraph! SwFlyPortion doesn't
    // have a pointer sadly so no way to find out.
    SwTwips nHeight(0);
    for (SwLineLayout const* pLine = pPara; pLine; pLine = pLine->GetNext())
    {
        nHeight += pLine->Height();
        if (::sw::FindNonFlyPortion(*pLine))
        {
            break;
        }
    }
    return nHeight;
}

sal_Int32 SwTextFrame::GetLineCount(TextFrameIndex const nPos)
{
    sal_Int32 nRet = 0;
    SwTextFrame *pFrame = this;
    do
    {
        pFrame->GetFormatted();
        if( !pFrame->HasPara() )
            break;
        SwTextSizeInfo aInf( pFrame );
        SwTextMargin aLine( pFrame, &aInf );
        if (TextFrameIndex(COMPLETE_STRING) == nPos)
            aLine.Bottom();
        else
            aLine.CharToLine( nPos );
        nRet = nRet + aLine.GetLineNr();
        pFrame = pFrame->GetFollow();
    } while ( pFrame && pFrame->GetOffset() <= nPos );
    return nRet;
}

void SwTextFrame::ChgThisLines()
{
    // not necessary to format here (GetFormatted etc.), because we have to come from there!
    sal_Int32 nNew = 0;
    const SwLineNumberInfo &rInf = GetDoc().GetLineNumberInfo();
    if ( !GetText().isEmpty() && HasPara() )
    {
        SwTextSizeInfo aInf( this );
        SwTextMargin aLine( this, &aInf );
        if ( rInf.IsCountBlankLines() )
        {
            aLine.Bottom();
            nNew = aLine.GetLineNr();
        }
        else
        {
            do
            {
                if( aLine.GetCurr()->HasContent() )
                    ++nNew;
            } while ( aLine.NextLine() );
        }
    }
    else if ( rInf.IsCountBlankLines() )
        nNew = 1;

    if ( nNew == mnThisLines )
        return;

    if (!IsInTab() && GetTextNodeForParaProps()->GetSwAttrSet().GetLineNumber().IsCount())
    {
        mnAllLines -= mnThisLines;
        mnThisLines = nNew;
        mnAllLines  += mnThisLines;
        SwFrame *pNxt = GetNextContentFrame();
        while( pNxt && pNxt->IsInTab() )
        {
            pNxt = pNxt->FindTabFrame();
            if( nullptr != pNxt )
                pNxt = pNxt->FindNextCnt();
        }
        if( pNxt )
            pNxt->InvalidateLineNum();

        // Extend repaint to the bottom.
        if ( HasPara() )
        {
            SwRepaint& rRepaint = GetPara()->GetRepaint();
            rRepaint.Bottom( std::max( rRepaint.Bottom(),
                                   getFrameArea().Top()+getFramePrintArea().Bottom()));
        }
    }
    else // Paragraphs which are not counted should not manipulate the AllLines.
        mnThisLines = nNew;
}

void SwTextFrame::RecalcAllLines()
{
    ValidateLineNum();

    if ( IsInTab() )
        return;

    const sal_Int32 nOld = GetAllLines();
    const SwFormatLineNumber &rLineNum = GetTextNodeForParaProps()->GetSwAttrSet().GetLineNumber();
    sal_Int32 nNewNum;
    const bool bRestart = GetDoc().GetLineNumberInfo().IsRestartEachPage();

    if ( !IsFollow() && rLineNum.GetStartValue() && rLineNum.IsCount() )
        nNewNum = rLineNum.GetStartValue() - 1;
    // If it is a follow or not has not be considered if it is a restart at each page; the
    // restart should also take effect at follows.
    else if ( bRestart && FindPageFrame()->FindFirstBodyContent() == this )
    {
        nNewNum = 0;
    }
    else
    {
        SwContentFrame *pPrv = GetPrevContentFrame();
        while ( pPrv &&
                (pPrv->IsInTab() || pPrv->IsInDocBody() != IsInDocBody()) )
            pPrv = pPrv->GetPrevContentFrame();

        // i#78254 Restart line numbering at page change
        // First body content may be in table!
        if ( bRestart && pPrv && pPrv->FindPageFrame() != FindPageFrame() )
            pPrv = nullptr;

        nNewNum = pPrv ? static_cast<SwTextFrame*>(pPrv)->GetAllLines() : 0;
    }
    if ( rLineNum.IsCount() )
        nNewNum += GetThisLines();

    if ( nOld == nNewNum )
        return;

    mnAllLines = nNewNum;
    SwContentFrame *pNxt = GetNextContentFrame();
    while ( pNxt &&
            (pNxt->IsInTab() || pNxt->IsInDocBody() != IsInDocBody()) )
        pNxt = pNxt->GetNextContentFrame();
    if ( pNxt )
    {
        if ( pNxt->GetUpper() != GetUpper() )
            pNxt->InvalidateLineNum();
        else
            pNxt->InvalidateLineNum_();
    }
}

void SwTextFrame::VisitPortions( SwPortionHandler& rPH ) const
{
    const SwParaPortion* pPara = isFrameAreaDefinitionValid() ? GetPara() : nullptr;

    if (pPara)
    {
        if ( IsFollow() )
            rPH.Skip( GetOffset() );

        const SwLineLayout* pLine = pPara;
        while ( pLine )
        {
            const SwLinePortion* pPor = pLine->GetFirstPortion();
            while ( pPor )
            {
                pPor->HandlePortion( rPH );
                pPor = pPor->GetNextPortion();
            }

            rPH.LineBreak();
            pLine = pLine->GetNext();
        }
    }

    rPH.Finish();
}

const SwScriptInfo* SwTextFrame::GetScriptInfo() const
{
    const SwParaPortion* pPara = GetPara();
    return pPara ? &pPara->GetScriptInfo() : nullptr;
}

SwScriptInfo* SwTextFrame::GetScriptInfo()
{
    SwParaPortion* pPara = GetPara();
    return pPara ? &pPara->GetScriptInfo() : nullptr;
}

/**
 * Helper function for SwTextFrame::CalcBasePosForFly()
 */
static SwTwips lcl_CalcFlyBasePos( const SwTextFrame& rFrame, SwRect aFlyRect,
                            SwTextFly const & rTextFly )
{
    SwRectFnSet aRectFnSet(&rFrame);
    SwTwips nRet = rFrame.IsRightToLeft() ?
                   aRectFnSet.GetRight(rFrame.getFrameArea()) :
                   aRectFnSet.GetLeft(rFrame.getFrameArea());

    do
    {
        SwRect aRect = rTextFly.GetFrame( aFlyRect );
        if ( 0 != aRectFnSet.GetWidth(aRect) )
        {
            if ( rFrame.IsRightToLeft() )
            {
                if ( aRectFnSet.GetRight(aRect) -
                     aRectFnSet.GetRight(aFlyRect) >= 0 )
                {
                    aRectFnSet.SetRight(
aFlyRect,                         aRectFnSet.GetLeft(aRect) );
                    nRet = aRectFnSet.GetLeft(aRect);
                }
                else
                    break;
            }
            else
            {
                if ( aRectFnSet.GetLeft(aFlyRect) -
                     aRectFnSet.GetLeft(aRect) >= 0 )
                {
                    aRectFnSet.SetLeft(
aFlyRect,                         aRectFnSet.GetRight(aRect) + 1 );
                    nRet = aRectFnSet.GetRight(aRect);
                }
                else
                    break;
            }
        }
        else
            break;
    }
    while ( aRectFnSet.GetWidth(aFlyRect) > 0 );

    return nRet;
}

void SwTextFrame::CalcBaseOfstForFly()
{
    OSL_ENSURE( !IsVertical() || !IsSwapped(),
            "SwTextFrame::CalcBasePosForFly with swapped frame!" );

    if (!GetDoc().getIDocumentSettingAccess().get(DocumentSettingId::ADD_FLY_OFFSETS))
        return;

    SwRectFnSet aRectFnSet(this);

    SwRect aFlyRect( getFrameArea().Pos() + getFramePrintArea().Pos(), getFramePrintArea().SSize() );

    // Get first 'real' line and adjust position and height of line rectangle.
    // Correct behaviour if no 'real' line exists
    // (empty paragraph with and without a dummy portion)
    SwTwips nFlyAnchorVertOfstNoWrap = 0;
    {
        SwTwips nTop = aRectFnSet.GetTop(aFlyRect);
        const SwLineLayout* pLay = GetPara();
        SwTwips nLineHeight = 200;
        while( pLay && pLay->IsDummy() && pLay->GetNext() )
        {
            nTop += pLay->Height();
            nFlyAnchorVertOfstNoWrap += pLay->Height();
            pLay = pLay->GetNext();
        }
        if ( pLay )
        {
            nLineHeight = pLay->Height();
        }
        aRectFnSet.SetTopAndHeight( aFlyRect, nTop, nLineHeight );
    }

    SwTextFly aTextFly( this );
    aTextFly.SetIgnoreCurrentFrame( true );
    aTextFly.SetIgnoreContour( true );
    // ignore objects in page header|footer for
    // text frames not in page header|footer
    aTextFly.SetIgnoreObjsInHeaderFooter( true );
    SwTwips nRet1 = lcl_CalcFlyBasePos( *this, aFlyRect, aTextFly );
    aTextFly.SetIgnoreCurrentFrame( false );
    SwTwips nRet2 = lcl_CalcFlyBasePos( *this, aFlyRect, aTextFly );

    // make values relative to frame start position
    SwTwips nLeft = IsRightToLeft() ?
                    aRectFnSet.GetRight(getFrameArea()) :
                    aRectFnSet.GetLeft(getFrameArea());

    mnFlyAnchorOfst = nRet1 - nLeft;
    mnFlyAnchorOfstNoWrap = nRet2 - nLeft;

    if (!GetDoc().getIDocumentSettingAccess().get(DocumentSettingId::ADD_VERTICAL_FLY_OFFSETS))
        return;

    if (mnFlyAnchorOfstNoWrap > 0)
        mnFlyAnchorVertOfstNoWrap = nFlyAnchorVertOfstNoWrap;
}

SwTwips SwTextFrame::GetBaseVertOffsetForFly(bool bIgnoreFlysAnchoredAtThisFrame) const
{
    return bIgnoreFlysAnchoredAtThisFrame ? 0 : mnFlyAnchorVertOfstNoWrap;
}

/**
 * Repaint all text frames of the given text node
 */
void SwTextFrame::repaintTextFrames( const SwTextNode& rNode )
{
    SwIterator<SwTextFrame, SwTextNode, sw::IteratorMode::UnwrapMulti> aIter(rNode);
    for( const SwTextFrame *pFrame = aIter.First(); pFrame; pFrame = aIter.Next() )
    {
        SwRect aRec( pFrame->GetPaintArea() );
        const SwRootFrame *pRootFrame = pFrame->getRootFrame();
        SwViewShell *pCurShell = pRootFrame ? pRootFrame->GetCurrShell() : nullptr;
        if( pCurShell )
            pCurShell->InvalidateWindows( aRec );
    }
}

void SwTextFrame::UpdateOutlineContentVisibilityButton(SwWrtShell* pWrtSh) const
{
    if (pWrtSh && pWrtSh->GetViewOptions()->IsShowOutlineContentVisibilityButton() &&
            GetTextNodeFirst()->IsOutline())
    {
        SwEditWin& rEditWin = pWrtSh->GetView().GetEditWin();
        SwFrameControlsManager& rMngr = rEditWin.GetFrameControlsManager();
        rMngr.SetOutlineContentVisibilityButton(this);
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
