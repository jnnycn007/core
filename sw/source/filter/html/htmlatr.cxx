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

#include <hintids.hxx>
#include <com/sun/star/i18n/ScriptType.hpp>
#include <com/sun/star/i18n/XBreakIterator.hpp>
#include <comphelper/string.hxx>
#include <utility>
#include <vcl/svapp.hxx>
#include <svtools/htmlout.hxx>
#include <svtools/htmlkywd.hxx>
#include <svtools/htmltokn.h>
#include <svl/whiter.hxx>
#include <sfx2/event.hxx>
#include <sfx2/htmlmode.hxx>
#include <editeng/escapementitem.hxx>
#include <editeng/formatbreakitem.hxx>
#include <editeng/boxitem.hxx>
#include <editeng/ulspitem.hxx>
#include <editeng/udlnitem.hxx>
#include <editeng/crossedoutitem.hxx>
#include <editeng/blinkitem.hxx>
#include <editeng/colritem.hxx>
#include <editeng/fontitem.hxx>
#include <editeng/fhgtitem.hxx>
#include <editeng/postitem.hxx>
#include <editeng/wghtitem.hxx>
#include <editeng/adjustitem.hxx>
#include <editeng/lrspitem.hxx>
#include <editeng/langitem.hxx>
#include <editeng/frmdiritem.hxx>
#include <fchrfmt.hxx>
#include <fmtautofmt.hxx>
#include <fmtfsize.hxx>
#include <fmtclds.hxx>
#include <fmtpdsc.hxx>
#include <fmtflcnt.hxx>
#include <fmtinfmt.hxx>
#include <txatbase.hxx>
#include <frmatr.hxx>
#include <charfmt.hxx>
#include <fmtfld.hxx>
#include <doc.hxx>
#include <IDocumentStylePoolAccess.hxx>
#include <pam.hxx>
#include <ndtxt.hxx>
#include <paratr.hxx>
#include <poolfmt.hxx>
#include <pagedesc.hxx>
#include <swtable.hxx>
#include <fldbas.hxx>
#include <breakit.hxx>
#include "htmlatr.hxx"
#include "htmlnum.hxx"
#include "wrthtml.hxx"
#include "htmlfly.hxx"
#include <numrule.hxx>
#include <rtl/character.hxx>
#include <osl/diagnose.h>
#include <deque>

#include <svtools/HtmlWriter.hxx>
#include <o3tl/string_view.hxx>

#include <memory>
#include <algorithm>

using namespace css;

HTMLOutEvent const aAnchorEventTable[] =
{
    { OOO_STRING_SVTOOLS_HTML_O_SDonclick,      OOO_STRING_SVTOOLS_HTML_O_onclick,      SvMacroItemId::OnClick },
    { OOO_STRING_SVTOOLS_HTML_O_SDonmouseover,  OOO_STRING_SVTOOLS_HTML_O_onmouseover,  SvMacroItemId::OnMouseOver  },
    { OOO_STRING_SVTOOLS_HTML_O_SDonmouseout,   OOO_STRING_SVTOOLS_HTML_O_onmouseout,   SvMacroItemId::OnMouseOut   },
    { nullptr, nullptr, SvMacroItemId::NONE }
};

static SwHTMLWriter& OutHTML_SvxAdjust( SwHTMLWriter& rWrt, const SfxPoolItem& rHt );

sal_uInt16 SwHTMLWriter::GetDefListLvl( const UIName& rNm, sal_uInt16 nPoolId )
{
    if( nPoolId == RES_POOLCOLL_HTML_DD )
    {
        return 1 | HTML_DLCOLL_DD;
    }
    else if( nPoolId == RES_POOLCOLL_HTML_DT )
    {
        return 1 | HTML_DLCOLL_DT;
    }

    OUString sDTDD = u"" OOO_STRING_SVTOOLS_HTML_dt " "_ustr;
    if( o3tl::starts_with(rNm.toString(), sDTDD) )
        // DefinitionList - term
        return o3tl::narrowing<sal_uInt16>(o3tl::toInt32(rNm.toString().subView( sDTDD.getLength() ))) | HTML_DLCOLL_DT;

    sDTDD = OOO_STRING_SVTOOLS_HTML_dd " ";
    if( o3tl::starts_with(rNm.toString(), sDTDD) )
        // DefinitionList - definition
        return o3tl::narrowing<sal_uInt16>(o3tl::toInt32(rNm.toString().subView( sDTDD.getLength() ))) | HTML_DLCOLL_DD;

    return 0;
}

void SwHTMLWriter::OutAndSetDefList( sal_uInt16 nNewLvl )
{
    // possibly, we first need to start a new list
    if( m_nDefListLvl < nNewLvl )
    {
        // output </pre> for the previous(!) paragraph, if required.
        // Preferable, the <pre> is exported by OutHTML_SwFormatOff for the
        // previous  paragraph already, but that's not possible, because a very
        // deep look at the next paragraph (this one) is required to figure
        // out that a def list starts here.

        ChangeParaToken( HtmlTokenId::NONE );

        // write according to the level difference
        for( sal_uInt16 i=m_nDefListLvl; i<nNewLvl; i++ )
        {
            if (IsLFPossible())
                OutNewLine();
            HTMLOutFuncs::Out_AsciiTag( Strm(), Concat2View(GetNamespace() + OOO_STRING_SVTOOLS_HTML_deflist) );
            HTMLOutFuncs::Out_AsciiTag( Strm(), Concat2View(GetNamespace() + OOO_STRING_SVTOOLS_HTML_dd) );
            IncIndentLevel();
            SetLFPossible(true);
        }
    }
    else if( m_nDefListLvl > nNewLvl )
    {
        for( sal_uInt16 i=nNewLvl ; i < m_nDefListLvl; i++ )
        {
            DecIndentLevel();
            if (IsLFPossible())
                OutNewLine();
            HTMLOutFuncs::Out_AsciiTag( Strm(), Concat2View(GetNamespace() + OOO_STRING_SVTOOLS_HTML_dd), false );
            HTMLOutFuncs::Out_AsciiTag( Strm(), Concat2View(GetNamespace() + OOO_STRING_SVTOOLS_HTML_deflist), false );
            SetLFPossible(true);
        }
    }

    m_nDefListLvl = nNewLvl;
}

void SwHTMLWriter::ChangeParaToken( HtmlTokenId nNew )
{
    if( nNew != m_nLastParaToken && HtmlTokenId::PREFORMTXT_ON == m_nLastParaToken )
    {
        HTMLOutFuncs::Out_AsciiTag( Strm(), Concat2View(GetNamespace() + OOO_STRING_SVTOOLS_HTML_preformtxt), false );
        SetLFPossible(true);
    }
    m_nLastParaToken = nNew;
}

sal_uInt16 SwHTMLWriter::GetCSS1ScriptForScriptType( sal_uInt16 nScriptType )
{
    sal_uInt16 nRet = CSS1_OUTMODE_ANY_SCRIPT;

    switch( nScriptType )
    {
    case i18n::ScriptType::LATIN:
        nRet = CSS1_OUTMODE_WESTERN;
        break;
    case i18n::ScriptType::ASIAN:
        nRet = CSS1_OUTMODE_CJK;
        break;
    case i18n::ScriptType::COMPLEX:
        nRet = CSS1_OUTMODE_CTL;
        break;
    }

    return nRet;
}

// a single output function should be enough for all formats
/*
 * Output the formats as follows
 * - output the tag for formats for which a corresponding HTML tag exist
 * - for all the other formats, output a paragraph tag <P> and set bUserFormat
 * - if a paragraph alignment is set for the supplied ItemSet of the node or
 *   for the ItemSet of the format, output an ALIGN=xxx if HTML allows it
 * - In all cases, hard attribute is written as STYLE option.
 *   If bUserFormat is not set, only the supplied ItemSet is considered.
 *   Otherwise, attributes of the format are output as well.
 */

namespace {

struct SwHTMLTextCollOutputInfo
{
    OString aToken;        // End token to be output
    std::optional<SfxItemSet> moItemSet;    // hard attribute

    bool bInNumberBulletList;         // in an enumerated list;
    bool bParaPossible;         // a </P> may be output additionally
    bool bOutPara;              // a </P> is supposed to be output
    bool bOutDiv;               // write a </DIV>

    SwHTMLTextCollOutputInfo() :
        bInNumberBulletList( false ),
        bParaPossible( false ),
        bOutPara( false ),
        bOutDiv( false )
    {}

    bool HasParaToken() const { return aToken == OOO_STRING_SVTOOLS_HTML_parabreak; }
    bool ShouldOutputToken() const { return bOutPara || !HasParaToken(); }
};

}

SwHTMLFormatInfo::SwHTMLFormatInfo( const SwFormat *pF, SwDoc *pDoc, SwDoc *pTemplate,
                              bool bOutStyles,
                              LanguageType eDfltLang,
                              sal_uInt16 nCSS1Script )
    : pFormat(pF)
    , nLeftMargin(0)
    , nRightMargin(0)
    , nFirstLineIndent(0)
    , nTopMargin(0)
    , nBottomMargin(0)
    , bScriptDependent( false )
{
    sal_uInt16 nRefPoolId = 0;
    // Get the selector of the format
    sal_uInt16 nDeep = SwHTMLWriter::GetCSS1Selector( pFormat, aToken, aClass,
                                                  nRefPoolId );
    OSL_ENSURE( nDeep ? !aToken.isEmpty() : aToken.isEmpty(),
            "Something seems to be wrong with this token!" );
    OSL_ENSURE( nDeep ? nRefPoolId != 0 : nRefPoolId == 0,
            "Something seems to be wrong with the comparison style!" );

    bool bTextColl = pFormat->Which() == RES_TXTFMTCOLL ||
                    pFormat->Which() == RES_CONDTXTFMTCOLL;

    const SwFormat *pReferenceFormat = nullptr; // Comparison format
    if( nDeep != 0 )
    {
        // It's an HTML-tag style or this style is derived from such
        // a style.
        if( !bOutStyles )
        {
            // if no styles are exported, it may be necessary to additionally
            // write hard attribute
            switch( nDeep )
            {
            case CSS1_FMT_ISTAG:
            case CSS1_FMT_CMPREF:
                // for HTML-tag styles the differences to the original
                // (if available)
                pReferenceFormat = SwHTMLWriter::GetTemplateFormat( nRefPoolId,
                                                        &pTemplate->getIDocumentStylePoolAccess() );
                break;

            default:
                // otherwise, the differences to the HTML-tag style of the
                // original or the ones to the current document, if it the
                // HTML-tag style is not available
                if( pTemplate )
                    pReferenceFormat = SwHTMLWriter::GetTemplateFormat( nRefPoolId,
                                                            &pTemplate->getIDocumentStylePoolAccess() );
                else
                    pReferenceFormat = SwHTMLWriter::GetParentFormat( *pFormat, nDeep );
                break;
            }
        }
    }
    else if( bTextColl )
    {
        // HTML-tag styles that are not derived from a paragraph style
        // must be exported as hard attribute relative to the text-body
        // style. For a 'not-styles' export, the one of the HTML style
        // should be used as a reference
        if( !bOutStyles && pTemplate )
            pReferenceFormat = pTemplate->getIDocumentStylePoolAccess().GetTextCollFromPool( RES_POOLCOLL_TEXT, false );
        else
            pReferenceFormat = pDoc->getIDocumentStylePoolAccess().GetTextCollFromPool( RES_POOLCOLL_TEXT, false );
    }

    if( pReferenceFormat || nDeep==0 )
    {
        moItemSet.emplace( *pFormat->GetAttrSet().GetPool(),
                                       pFormat->GetAttrSet().GetRanges() );
        // if the differences to a different style are supposed to be
        // written, hard attribute is necessary. This is always true
        // for styles that are not derived from HTML-tag styles.

        moItemSet->Set( pFormat->GetAttrSet() );

        if( pReferenceFormat )
            SwHTMLWriter::SubtractItemSet( *moItemSet, pReferenceFormat->GetAttrSet(), true );

        // delete ItemSet that is empty straight away. This will save work
        // later on
        if( !moItemSet->Count() )
        {
            moItemSet.reset();
        }
    }

    if( !bTextColl )
        return;

    if( bOutStyles )
    {
        // We have to add hard attributes for any script dependent
        // item that is not accessed by the style
        static const sal_uInt16 aWhichIds[3][4] =
        {
            { RES_CHRATR_FONT, RES_CHRATR_FONTSIZE,
                RES_CHRATR_POSTURE, RES_CHRATR_WEIGHT },
            { RES_CHRATR_CJK_FONT, RES_CHRATR_CJK_FONTSIZE,
                RES_CHRATR_CJK_POSTURE, RES_CHRATR_CJK_WEIGHT },
            { RES_CHRATR_CTL_FONT, RES_CHRATR_CTL_FONTSIZE,
                RES_CHRATR_CTL_POSTURE, RES_CHRATR_CTL_WEIGHT }
        };

        sal_uInt16 nRef = 0;
        sal_uInt16 aSets[2] = {0,0};
        switch( nCSS1Script )
        {
        case CSS1_OUTMODE_WESTERN:
            nRef = 0;
            aSets[0] = 1;
            aSets[1] = 2;
            break;
        case CSS1_OUTMODE_CJK:
            nRef = 1;
            aSets[0] = 0;
            aSets[1] = 2;
            break;
        case CSS1_OUTMODE_CTL:
            nRef = 2;
            aSets[0] = 0;
            aSets[1] = 1;
            break;
        }
        for( int i=0; i<4; ++i )
        {
            const SfxPoolItem& rRef = pFormat->GetFormatAttr( aWhichIds[nRef][i] );
            for(sal_uInt16 nSet : aSets)
            {
                const SfxPoolItem& rSet = pFormat->GetFormatAttr( aWhichIds[nSet][i] );
                if( rSet != rRef )
                {
                    if( !moItemSet )
                        moItemSet.emplace( *pFormat->GetAttrSet().GetPool(),
                                                   pFormat->GetAttrSet().GetRanges() );
                    moItemSet->Put( rSet );
                }
            }
        }
    }

    // remember all the different default spacings from the style or
    // the comparison style.
    SvxFirstLineIndentItem const& rFirstLine(
        (pReferenceFormat ? pReferenceFormat : pFormat)->GetFirstLineIndent());
    SvxTextLeftMarginItem const& rTextLeftMargin(
        (pReferenceFormat ? pReferenceFormat : pFormat)->GetTextLeftMargin());
    SvxRightMarginItem const& rRightMargin(
        (pReferenceFormat ? pReferenceFormat : pFormat)->GetRightMargin());
    nLeftMargin = rTextLeftMargin.ResolveTextLeft({});
    nRightMargin = rRightMargin.ResolveRight({});
    nFirstLineIndent = rFirstLine.ResolveTextFirstLineOffset({});

    const SvxULSpaceItem &rULSpace =
        (pReferenceFormat ? pReferenceFormat : pFormat)->GetULSpace();
    nTopMargin = rULSpace.GetUpper();
    nBottomMargin = rULSpace.GetLower();

    // export language if it differs from the default language
    TypedWhichId<SvxLanguageItem> nWhichId =
        SwHTMLWriter::GetLangWhichIdFromScript( nCSS1Script );
    const SvxLanguageItem& rLang = pFormat->GetFormatAttr( nWhichId );
    LanguageType eLang = rLang.GetLanguage();
    if( eLang != eDfltLang )
    {
        if( !moItemSet )
            moItemSet.emplace( *pFormat->GetAttrSet().GetPool(),
                                       pFormat->GetAttrSet().GetRanges() );
        moItemSet->Put( rLang );
    }

    static const TypedWhichId<SvxLanguageItem> aWhichIds[3] =
        { RES_CHRATR_LANGUAGE, RES_CHRATR_CJK_LANGUAGE,
            RES_CHRATR_CTL_LANGUAGE };
    for(const TypedWhichId<SvxLanguageItem>& i : aWhichIds)
    {
        if( i != nWhichId )
        {
            const SvxLanguageItem& rTmpLang = pFormat->GetFormatAttr(i);
            if( rTmpLang.GetLanguage() != eLang )
            {
                if( !moItemSet )
                    moItemSet.emplace( *pFormat->GetAttrSet().GetPool(),
                                               pFormat->GetAttrSet().GetRanges() );
                moItemSet->Put( rTmpLang );
            }
        }
    }

}

SwHTMLFormatInfo::~SwHTMLFormatInfo()
{
}

static void OutHTML_SwFormat( SwHTMLWriter& rWrt, const SwFormat& rFormat,
                    const SfxItemSet *pNodeItemSet,
                    SwHTMLTextCollOutputInfo& rInfo )
{
    OSL_ENSURE( RES_CONDTXTFMTCOLL==rFormat.Which() || RES_TXTFMTCOLL==rFormat.Which(),
            "not a paragraph style" );

    // First, some flags
    sal_uInt16 nNewDefListLvl = 0;
    sal_uInt16 nNumStart = USHRT_MAX;
    bool bForceDL = false;
    bool bDT = false;
    rInfo.bInNumberBulletList = false;    // Are we in a list?
    bool bNumbered = false;         // The current paragraph is numbered
    bool bPara = false;             // the current token is <P>
    bool bHeading = false;          // the current token is <H1> .. <H6>
    rInfo.bParaPossible = false;    // a <P> may be additionally output
    bool bNoEndTag = false;         // don't output an end tag

    rWrt.m_bNoAlign = false;       // no ALIGN=... possible

    if (rWrt.mbXHTML)
    {
        rWrt.m_bNoAlign = true;
    }

    sal_uInt8 nBulletGrfLvl = 255;  // The bullet graphic we want to output

    // Are we in a bulleted or numbered list?
    const SwTextNode* pTextNd = rWrt.m_pCurrentPam->GetPointNode().GetTextNode();

    SwHTMLNumRuleInfo aNumInfo;
    if( rWrt.GetNextNumInfo() )
    {
        aNumInfo = *rWrt.GetNextNumInfo();
        rWrt.ClearNextNumInfo();
    }
    else
    {
        aNumInfo.Set( *pTextNd );
    }

    if( aNumInfo.GetNumRule() )
    {
        rInfo.bInNumberBulletList = true;
        nNewDefListLvl = 0;

        // is the current paragraph numbered?
        bNumbered = aNumInfo.IsNumbered();
        sal_uInt8 nLvl = aNumInfo.GetLevel();

        OSL_ENSURE( pTextNd->GetActualListLevel() == nLvl,
                "Remembered Num level is wrong" );
        OSL_ENSURE( bNumbered == pTextNd->IsCountedInList(),
                "Remembered numbering state is wrong" );

        if( bNumbered )
        {
            nBulletGrfLvl = nLvl; // only temporarily!!!
            // #i57919#
            // correction of re-factoring done by cws swnumtree:
            // - <nNumStart> has to contain the restart value, if the
            //   numbering is restarted at this text node. Value <USHRT_MAX>
            //   indicates, that no additional restart value has to be written.
            if ( pTextNd->IsListRestart() )
            {
                nNumStart = static_cast< sal_uInt16 >(pTextNd->GetActualListStartValue());
            }
            OSL_ENSURE( rWrt.m_nLastParaToken == HtmlTokenId::NONE,
                "<PRE> was not closed before <LI>." );
        }
    }

    // Now, we're getting the token and, if necessary, the class
    std::unique_ptr<SwHTMLFormatInfo> pTmpInfo(new SwHTMLFormatInfo(&rFormat));
    SwHTMLFormatInfo *pFormatInfo;
    SwHTMLFormatInfos::iterator it = rWrt.m_TextCollInfos.find( pTmpInfo );
    if (it != rWrt.m_TextCollInfos.end())
    {
        pFormatInfo = it->get();
    }
    else
    {
        pFormatInfo = new SwHTMLFormatInfo( &rFormat, rWrt.m_pDoc, rWrt.m_xTemplate.get(),
                                      rWrt.m_bCfgOutStyles, rWrt.m_eLang,
                                      rWrt.m_nCSS1Script );
        rWrt.m_TextCollInfos.insert(std::unique_ptr<SwHTMLFormatInfo>(pFormatInfo));
        if( rWrt.m_aScriptParaStyles.count( rFormat.GetName().toString() ) )
            pFormatInfo->bScriptDependent = true;
    }

    // Now, we define what is possible due to the token
    HtmlTokenId nToken = HtmlTokenId::NONE;          // token for tag change
    bool bOutNewLine = false;   // only output a single LF?
    if( !pFormatInfo->aToken.isEmpty() )
    {
        // It is an HTML-tag style or the style is derived from such a
        // style.
        rInfo.aToken = pFormatInfo->aToken;

        if (rInfo.aToken == OOO_STRING_SVTOOLS_HTML_address)
        {
            rInfo.bParaPossible = true;
            rWrt.m_bNoAlign = true;
        }
        else if (rInfo.aToken == OOO_STRING_SVTOOLS_HTML_blockquote)
        {
            rInfo.bParaPossible = true;
            rWrt.m_bNoAlign = true;
        }
        else if (rInfo.aToken == OOO_STRING_SVTOOLS_HTML_parabreak)
        {
            bPara = true;
        }
        else if (rInfo.aToken == OOO_STRING_SVTOOLS_HTML_preformtxt)
        {
            if (HtmlTokenId::PREFORMTXT_ON == rWrt.m_nLastParaToken)
            {
                bOutNewLine = true;
            }
            else
            {
                nToken = HtmlTokenId::PREFORMTXT_ON;
                rWrt.m_bNoAlign = true;
                bNoEndTag = true;
            }
        }
        else if (rInfo.aToken == OOO_STRING_SVTOOLS_HTML_dt || rInfo.aToken == OOO_STRING_SVTOOLS_HTML_dd)
        {
            bDT = rInfo.aToken == OOO_STRING_SVTOOLS_HTML_dt;
            rInfo.bParaPossible = !bDT;
            rWrt.m_bNoAlign = true;
            bForceDL = true;
        }
        else if (rInfo.aToken == OOO_STRING_SVTOOLS_HTML_head1 ||
                 rInfo.aToken == OOO_STRING_SVTOOLS_HTML_head2 ||
                 rInfo.aToken == OOO_STRING_SVTOOLS_HTML_head3 ||
                 rInfo.aToken == OOO_STRING_SVTOOLS_HTML_head4 ||
                 rInfo.aToken == OOO_STRING_SVTOOLS_HTML_head5 ||
                 rInfo.aToken == OOO_STRING_SVTOOLS_HTML_head6)
        {
            bHeading = true;
        }
    }
    else
    {
        // all styles that do not correspond to an HTML tag, or that are
        // not derived from it, are exported as <P>

        rInfo.aToken = OOO_STRING_SVTOOLS_HTML_parabreak ""_ostr;
        bPara = true;
    }

    // If necessary, take the hard attribute from the style
    if( pFormatInfo->moItemSet )
    {
        OSL_ENSURE(!rInfo.moItemSet, "Where does this ItemSet come from?");
        rInfo.moItemSet.emplace( *pFormatInfo->moItemSet );
    }

    // additionally, add the hard attribute from the paragraph
    if( pNodeItemSet )
    {
        if (rInfo.moItemSet)
            rInfo.moItemSet->Put( *pNodeItemSet );
        else
            rInfo.moItemSet.emplace( *pNodeItemSet );
    }

    // we will need the lower spacing of the paragraph later on
    const SvxULSpaceItem& rULSpace =
        pNodeItemSet ? pNodeItemSet->Get(RES_UL_SPACE)
                     : rFormat.GetULSpace();

    if( (rWrt.m_bOutHeader &&
         rWrt.m_pCurrentPam->GetPoint()->GetNodeIndex() ==
            rWrt.m_pCurrentPam->GetMark()->GetNodeIndex()) ||
         rWrt.m_bOutFooter )
    {
        if( rWrt.m_bCfgOutStyles )
        {
            SvxULSpaceItem aULSpaceItem( rULSpace );
            if( rWrt.m_bOutHeader )
                aULSpaceItem.SetLower( rWrt.m_nHeaderFooterSpace );
            else
                aULSpaceItem.SetUpper( rWrt.m_nHeaderFooterSpace );

            if (!rInfo.moItemSet)
            {
                rInfo.moItemSet.emplace(*rFormat.GetAttrSet().GetPool(), svl::Items<RES_UL_SPACE, RES_UL_SPACE>);
            }
            rInfo.moItemSet->Put( aULSpaceItem );
        }
        rWrt.m_bOutHeader = false;
        rWrt.m_bOutFooter = false;
    }

    if( bOutNewLine )
    {
        // output a line break (without indentation) at the beginning of the
        // paragraph, only
        rInfo.aToken.clear();   // don't output an end tag
        rWrt.Strm().WriteOString( SAL_NEWLINE_STRING );

        return;
    }

    // should an ALIGN=... be written?
    const SvxAdjustItem* pAdjItem = nullptr;

    if( rInfo.moItemSet )
        pAdjItem = rInfo.moItemSet->GetItemIfSet( RES_PARATR_ADJUST, false );

    // Consider the lower spacing of the paragraph? (never in the last
    // paragraph of tables)
    bool bUseParSpace = !rWrt.m_bOutTable ||
                        (rWrt.m_pCurrentPam->GetPoint()->GetNodeIndex() !=
                         rWrt.m_pCurrentPam->GetMark()->GetNodeIndex());
    // If styles are exported, indented paragraphs become definition lists
    SvxFirstLineIndentItem const& rFirstLine(
        pNodeItemSet ? pNodeItemSet->Get(RES_MARGIN_FIRSTLINE)
                     : rFormat.GetFirstLineIndent());
    SvxTextLeftMarginItem const& rTextLeftMargin(
        pNodeItemSet ? pNodeItemSet->Get(RES_MARGIN_TEXTLEFT)
                     : rFormat.GetTextLeftMargin());
    if( (!rWrt.m_bCfgOutStyles || bForceDL) && !rInfo.bInNumberBulletList )
    {
        sal_Int32 nLeftMargin;
        if (bForceDL)
            nLeftMargin = rTextLeftMargin.ResolveTextLeft({});
        else
            nLeftMargin = rTextLeftMargin.ResolveTextLeft({}) > pFormatInfo->nLeftMargin
                              ? rTextLeftMargin.ResolveTextLeft({}) - pFormatInfo->nLeftMargin
                              : 0;

        if( nLeftMargin > 0 && rWrt.m_nDefListMargin > 0 )
        {
            nNewDefListLvl = static_cast< sal_uInt16 >((nLeftMargin + (rWrt.m_nDefListMargin/2)) /
                                                    rWrt.m_nDefListMargin);
            if( nNewDefListLvl == 0 && bForceDL && !bDT )
                nNewDefListLvl = 1;
        }
        else
        {
            // If the left margin is 0 or negative, emulating indent
            // with <dd> does not work. We then set a def list only if
            // the dd style is used.
            nNewDefListLvl = (bForceDL&& !bDT) ? 1 : 0;
        }

        bool bIsNextTextNode =
            rWrt.m_pDoc->GetNodes()[rWrt.m_pCurrentPam->GetPoint()->GetNodeIndex()+1]
                     ->IsTextNode();

        if( bForceDL && bDT )
        {
            // Instead of a DD we must use a DT from the level above this one.
            nNewDefListLvl++;
        }
        else if( !nNewDefListLvl && !rWrt.m_bCfgOutStyles && bPara &&
                 rULSpace.GetLower()==0 &&
                 ((bUseParSpace && bIsNextTextNode) || rWrt.m_nDefListLvl==1) &&
                 (!pAdjItem || SvxAdjust::Left==pAdjItem->GetAdjust()) )
        {
            // Export paragraphs without a lower spacing as DT
            nNewDefListLvl = 1;
            bDT = true;
            rInfo.bParaPossible = false;
            rWrt.m_bNoAlign = true;
        }
    }

    if( nNewDefListLvl != rWrt.m_nDefListLvl )
        rWrt.OutAndSetDefList( nNewDefListLvl );

    // if necessary, start a bulleted or numbered list
    if( rInfo.bInNumberBulletList )
    {
        OSL_ENSURE( !rWrt.m_nDefListLvl, "DL cannot be inside OL!" );
        OutHTML_NumberBulletListStart( rWrt, aNumInfo );

        if( bNumbered )
        {
            // disable PVS False positive 557 "Array underrun is possible"
            // we know here that nBulletGrfLvl < 10
            // we're in the case bInNumberBulletList && bNumbered
            // so we retrieved nLvl which comes from SwHTMLNumRuleInfo::GetLevel()
            if( !rWrt.m_aBulletGrfs[nBulletGrfLvl].isEmpty()  ) //-V557
                bNumbered = false;
            else
                nBulletGrfLvl = 255;
        }
    }

    // Take the defaults of the style, because they don't need to be
    // exported
    rWrt.m_nDfltLeftMargin = pFormatInfo->nLeftMargin;
    rWrt.m_nDfltRightMargin = pFormatInfo->nRightMargin;
    rWrt.m_nDfltFirstLineIndent = pFormatInfo->nFirstLineIndent;

    if( rInfo.bInNumberBulletList )
    {
        if (!rWrt.IsHTMLMode(HTMLMODE_LSPACE_IN_NUMBER_BULLET))
            rWrt.m_nDfltLeftMargin = rTextLeftMargin.ResolveTextLeft({});

        // In numbered lists, don't output a first line indent.
        rWrt.m_nFirstLineIndent = rFirstLine.ResolveTextFirstLineOffset({});
    }

    if( rInfo.bInNumberBulletList && bNumbered && bPara && !rWrt.m_bCfgOutStyles )
    {
        // a single LI doesn't have spacing
        rWrt.m_nDfltTopMargin = 0;
        rWrt.m_nDfltBottomMargin = 0;
    }
    else if( rWrt.m_nDefListLvl && bPara )
    {
        // a single DD doesn't have spacing, as well
        rWrt.m_nDfltTopMargin = 0;
        rWrt.m_nDfltBottomMargin = 0;
    }
    else
    {
        rWrt.m_nDfltTopMargin = pFormatInfo->nTopMargin;
        // if in the last paragraph of a table the lower paragraph spacing
        // is changed, Netscape doesn't get it. That's why we don't
        // export anything here for now, by setting this spacing to the
        // default value.
        if( rWrt.m_bCfgNetscape4 && !bUseParSpace )
            rWrt.m_nDfltBottomMargin = rULSpace.GetLower();
        else
            rWrt.m_nDfltBottomMargin = pFormatInfo->nBottomMargin;
    }

    if( rWrt.m_nDefListLvl )
    {
        rWrt.m_nLeftMargin =
            (rWrt.m_nDefListLvl-1) * rWrt.m_nDefListMargin;
    }

    if (rWrt.IsLFPossible() && !rWrt.m_bFirstLine)
        rWrt.OutNewLine(); // paragraph tag on a new line
    rInfo.bOutPara = false;

    // this is now our new token
    rWrt.ChangeParaToken( nToken );

    bool bHasParSpace = bUseParSpace && rULSpace.GetLower() > 0;
    // XHTML doesn't allow character children for <blockquote>.
    bool bXhtmlBlockQuote = rWrt.mbXHTML && rInfo.aToken == OOO_STRING_SVTOOLS_HTML_blockquote;

    // if necessary, start a new list item
    bool bNumberedForListItem = bNumbered;
    if (!bNumberedForListItem)
    {
        // Open a list also for the leading unnumbered nodes (= list headers in ODF terminology);
        // to do that, detect if this unnumbered node is the first in this list
        const auto& rPrevListInfo = rWrt.GetNumInfo();
        if (rPrevListInfo.GetNumRule() != aNumInfo.GetNumRule() || aNumInfo.IsRestart(rPrevListInfo)
            || rPrevListInfo.GetDepth() < aNumInfo.GetDepth())
            bNumberedForListItem = true;
    }
    if( rInfo.bInNumberBulletList && bNumberedForListItem )
    {
        OStringBuffer sOut(rWrt.GetNamespace() + OOO_STRING_SVTOOLS_HTML_li);
        if (!bNumbered)
        {
            // Handles list headers (<text:list-header> ODF element)
            sOut.append(" " OOO_STRING_SVTOOLS_HTML_O_style "=\"display: block\"");
        }
        else if (USHRT_MAX != nNumStart)
            sOut.append(" " OOO_STRING_SVTOOLS_HTML_O_value "=\"" + OString::number(nNumStart)
                        + "\"");
        HTMLOutFuncs::Out_AsciiTag(rWrt.Strm(), sOut);
    }

    if( rWrt.m_nDefListLvl > 0 && !bForceDL )
    {
        OString aTag = bDT ? OOO_STRING_SVTOOLS_HTML_dt : OOO_STRING_SVTOOLS_HTML_dd;
        HTMLOutFuncs::Out_AsciiTag( rWrt.Strm(), Concat2View(rWrt.GetNamespace() + aTag) );
    }

    if( pAdjItem &&
        rWrt.IsHTMLMode( HTMLMODE_NO_CONTROL_CENTERING ) &&
        rWrt.HasControls() )
    {
        // The align=... attribute does behave strange in netscape
        // if there are controls in a paragraph, because the control and
        // all text behind the control does not recognize this attribute.
        OString sOut = "<" + rWrt.GetNamespace() + OOO_STRING_SVTOOLS_HTML_division;
        rWrt.Strm().WriteOString( sOut );

        rWrt.m_bTextAttr = false;
        rWrt.m_bOutOpts = true;
        OutHTML_SvxAdjust( rWrt, *pAdjItem );
        rWrt.Strm().WriteChar( '>' );
        pAdjItem = nullptr;
        rWrt.m_bNoAlign = false;
        rInfo.bOutDiv = true;
        rWrt.IncIndentLevel();
        rWrt.SetLFPossible(true);
        rWrt.OutNewLine();
    }

    // for BLOCKQUOTE, ADDRESS and DD we output another paragraph token, if
    // - no styles are written and
    // - a lower spacing or a paragraph alignment exists
    // Also, XHTML does not allow character children in this context.
    OString aToken = rInfo.aToken;
    if( (!rWrt.m_bCfgOutStyles || rWrt.mbXHTML) && rInfo.bParaPossible && !bPara &&
        (bHasParSpace || bXhtmlBlockQuote || pAdjItem) )
    {
        HTMLOutFuncs::Out_AsciiTag( rWrt.Strm(), Concat2View(rWrt.GetNamespace() + rInfo.aToken) );
        aToken = OOO_STRING_SVTOOLS_HTML_parabreak ""_ostr;
        bPara = true;
        rWrt.m_bNoAlign = false;
    }

    LanguageType eLang;
    if (rInfo.moItemSet)
    {
        const SvxLanguageItem& rLangItem = rInfo.moItemSet->Get(SwHTMLWriter::GetLangWhichIdFromScript(rWrt.m_nCSS1Script));
        eLang = rLangItem.GetLanguage();
    }
    else
        eLang = rWrt.m_eLang;

    if( rInfo.moItemSet )
    {
        static const TypedWhichId<SvxLanguageItem> aWhichIds[3] = { RES_CHRATR_LANGUAGE, RES_CHRATR_CJK_LANGUAGE, RES_CHRATR_CTL_LANGUAGE };

        for(auto const & i : aWhichIds)
        {
            // export language if it differs from the default language only.
            const SvxLanguageItem* pTmpItem = rInfo.moItemSet->GetItemIfSet( i );
            if( pTmpItem && pTmpItem->GetLanguage() == eLang )
                rInfo.moItemSet->ClearItem( i );
        }
    }

    // and the text direction
    SvxFrameDirection nDir = rWrt.GetHTMLDirection(
            (pNodeItemSet ? pNodeItemSet->Get( RES_FRAMEDIR )
                          : rFormat.GetFrameDir() ).GetValue() );

    // We only write a <P>, if
    // - we are not inside OL/UL/DL, or
    // - the paragraph of an OL/UL is not numbered or
    // - styles are not exported and
    //      - a lower spacing, or
    //      - a paragraph alignment exists, or
    // - styles are exported and
    //      - the text body style was changed, or
    //      - a user format is exported, or
    //      - a paragraph attribute exists
    if( !bPara ||
        (!rInfo.bInNumberBulletList && !rWrt.m_nDefListLvl) ||
        (rInfo.bInNumberBulletList && !bNumbered) ||
        (!rWrt.m_bCfgOutStyles &&
         (bHasParSpace || bXhtmlBlockQuote || pAdjItem ||
          (eLang != LANGUAGE_DONTKNOW && eLang != rWrt.m_eLang))) ||
        nDir != rWrt.m_nDirection ||
        rWrt.m_bCfgOutStyles )
    {
        // now, options are output
        rWrt.m_bTextAttr = false;
        rWrt.m_bOutOpts = true;

        OString sOut = "<" + rWrt.GetNamespace() + aToken;

        if( eLang != LANGUAGE_DONTKNOW && eLang != rWrt.m_eLang )
        {
            rWrt.Strm().WriteOString( sOut );
            sOut = ""_ostr;
            rWrt.OutLanguage( eLang );
        }

        if( nDir != rWrt.m_nDirection )
        {
            if( !sOut.isEmpty() )
            {
                rWrt.Strm().WriteOString( sOut );
                sOut = ""_ostr;
            }
            rWrt.OutDirection( nDir );
        }

        if( rWrt.m_bCfgOutStyles &&
            (!pFormatInfo->aClass.isEmpty() || pFormatInfo->bScriptDependent) )
        {
            sOut += " " OOO_STRING_SVTOOLS_HTML_O_class "=\"";
            rWrt.Strm().WriteOString( sOut );
            sOut = ""_ostr;
            OUString aClass( pFormatInfo->aClass );
            if( pFormatInfo->bScriptDependent )
            {
                if( !aClass.isEmpty() )
                   aClass += "-";
                switch( rWrt.m_nCSS1Script )
                {
                case CSS1_OUTMODE_WESTERN:
                    aClass += "western";
                    break;
                case CSS1_OUTMODE_CJK:
                    aClass += "cjk";
                    break;
                case CSS1_OUTMODE_CTL:
                    aClass += "ctl";
                    break;
                }
            }
            HTMLOutFuncs::Out_String( rWrt.Strm(), aClass );
            sOut += "\"";
        }

        // set inline heading (heading in a text frame anchored as character and
        // formatted with frame style "Inline Heading")
        if( bHeading && rWrt.IsInlineHeading() )
            sOut += " " OOO_STRING_SVTOOLS_HTML_O_style "=\"display:inline;\"";

        rWrt.Strm().WriteOString( sOut );
        sOut = ""_ostr;

        std::string_view sStyle;
        if (rWrt.IsSpacePreserve())
        {
            if (rWrt.mbXHTML)
                rWrt.Strm().WriteOString(" xml:space=\"preserve\"");
            else
                sStyle = "white-space: pre-wrap";
        }

        // if necessary, output alignment
        if( !rWrt.m_bNoAlign && pAdjItem )
            OutHTML_SvxAdjust( rWrt, *pAdjItem );

        rWrt.m_bParaDotLeaders = bPara && rWrt.m_bCfgPrintLayout && rWrt.indexOfDotLeaders(
                pTextNd->GetAnyFormatColl().GetPoolFormatId(), pTextNd->GetText()) > -1;

        // and now, if necessary, the STYLE options
        if (rWrt.m_bCfgOutStyles && rInfo.moItemSet)
        {
            OutCSS1_ParaTagStyleOpt(rWrt, *rInfo.moItemSet, sStyle);
        }

        if (rWrt.m_bParaDotLeaders) {
            sOut += " " OOO_STRING_SVTOOLS_HTML_O_class "=\""
                sCSS2_P_CLASS_leaders "\"><"
                OOO_STRING_SVTOOLS_HTML_O_span;
            rWrt.Strm().WriteOString( sOut );
            sOut = ""_ostr;
        }

        rWrt.Strm().WriteChar( '>' );

        // is a </P> supposed to be written?
        rInfo.bOutPara =
            bPara &&
            ( rWrt.m_bCfgOutStyles || bHasParSpace );

        // if no end tag is supposed to be written, delete it
        if( bNoEndTag )
            rInfo.aToken.clear();
    }

    if( nBulletGrfLvl != 255 )
    {
        OSL_ENSURE( aNumInfo.GetNumRule(), "Where is the numbering gone???" );
        OSL_ENSURE( nBulletGrfLvl < MAXLEVEL, "There are not this many layers." );
        const SwNumFormat& rNumFormat = aNumInfo.GetNumRule()->Get(nBulletGrfLvl);
        OutHTML_BulletImage( rWrt, OOO_STRING_SVTOOLS_HTML_image, rNumFormat.GetBrush(),
                rWrt.m_aBulletGrfs[nBulletGrfLvl]);
    }

    rWrt.GetNumInfo() = aNumInfo;

    // reset the defaults
    rWrt.m_nDfltLeftMargin = 0;
    rWrt.m_nDfltRightMargin = 0;
    rWrt.m_nDfltFirstLineIndent = 0;
    rWrt.m_nDfltTopMargin = 0;
    rWrt.m_nDfltBottomMargin = 0;
    rWrt.m_nLeftMargin = 0;
    rWrt.m_nFirstLineIndent = 0;
}

static void OutHTML_SwFormatOff( SwHTMLWriter& rWrt, const SwHTMLTextCollOutputInfo& rInfo )
{
    // if there is no token, we don't need to output anything
    if( rInfo.aToken.isEmpty() )
    {
        rWrt.FillNextNumInfo();
        const SwHTMLNumRuleInfo& rNextInfo = *rWrt.GetNextNumInfo();
        // a bulleted list must be closed in PRE as well
        if( rInfo.bInNumberBulletList )
        {

            const SwHTMLNumRuleInfo& rNRInfo = rWrt.GetNumInfo();
            if( rNextInfo.GetNumRule() != rNRInfo.GetNumRule() ||
                rNextInfo.GetDepth() != rNRInfo.GetDepth() ||
                rNextInfo.IsNumbered() || rNextInfo.IsRestart(rNRInfo) )
                rWrt.ChangeParaToken( HtmlTokenId::NONE );
            OutHTML_NumberBulletListEnd( rWrt, rNextInfo );
        }
        else if( rNextInfo.GetNumRule() != nullptr )
            rWrt.ChangeParaToken( HtmlTokenId::NONE );

        return;
    }

    if( rInfo.ShouldOutputToken() )
    {
        if (rWrt.IsPrettyPrint() && rWrt.IsLFPossible())
            rWrt.OutNewLine( true );

        // if necessary, for BLOCKQUOTE, ADDRESS and DD another paragraph token
        // is output, if
        // - no styles are written and
        // - a lower spacing exists
        if( rInfo.bParaPossible && rInfo.bOutPara )
            HTMLOutFuncs::Out_AsciiTag( rWrt.Strm(), Concat2View(rWrt.GetNamespace() + OOO_STRING_SVTOOLS_HTML_parabreak), false );

        HTMLOutFuncs::Out_AsciiTag( rWrt.Strm(), Concat2View(rWrt.GetNamespace() + rInfo.aToken), false );
        rWrt.SetLFPossible(
            rInfo.aToken != OOO_STRING_SVTOOLS_HTML_dt &&
            rInfo.aToken != OOO_STRING_SVTOOLS_HTML_dd &&
            rInfo.aToken != OOO_STRING_SVTOOLS_HTML_li);
    }
    if( rInfo.bOutDiv )
    {
        rWrt.DecIndentLevel();
        if (rWrt.IsLFPossible())
            rWrt.OutNewLine();
        HTMLOutFuncs::Out_AsciiTag( rWrt.Strm(), Concat2View(rWrt.GetNamespace() + OOO_STRING_SVTOOLS_HTML_division), false );
        rWrt.SetLFPossible(true);
    }

    // if necessary, close the list item, then close a bulleted or numbered list
    if( rInfo.bInNumberBulletList )
    {
        rWrt.FillNextNumInfo();
        OutHTML_NumberBulletListEnd( rWrt, *rWrt.GetNextNumInfo() );
    }
}

namespace {

class HTMLStartEndPos
{
    sal_Int32 m_nStart;
    sal_Int32 m_nEnd;
    std::unique_ptr<SfxPoolItem> m_pItem;

public:

    HTMLStartEndPos( const SfxPoolItem& rItem, sal_Int32 nStt, sal_Int32 nE );

    const SfxPoolItem& GetItem() const { return *m_pItem; }

    void SetStart(sal_Int32 nStt) { m_nStart = nStt; }
    sal_Int32 GetStart() const { return m_nStart; }

    sal_Int32 GetEnd() const { return m_nEnd; }
    void SetEnd(sal_Int32 nE) { m_nEnd = nE; }
};

}

HTMLStartEndPos::HTMLStartEndPos(const SfxPoolItem& rItem, sal_Int32 nStt, sal_Int32 nE)
    : m_nStart(nStt)
    , m_nEnd(nE)
    , m_pItem(rItem.Clone())
{}

typedef std::map<sal_Int32, std::vector<HTMLStartEndPos*>> HTMLStartEndPositions;

namespace {

enum HTMLOnOffState { HTML_NOT_SUPPORTED,   // unsupported Attribute
                      HTML_REAL_VALUE,      // Attribute with value
                      HTML_ON_VALUE,        // Attribute is On-Tag
                      HTML_OFF_VALUE,       // Attribute is Off-Tag
                      HTML_CHRFMT_VALUE,    // Attribute for character format
                      HTML_COLOR_VALUE,     // Attribute for foreground color
                      HTML_STYLE_VALUE,     // Attribute must be exported as style
                      HTML_DROPCAP_VALUE,   // DropCap-Attribute
                      HTML_AUTOFMT_VALUE }; // Attribute for automatic character styles

class HTMLEndPosLst
{
    HTMLStartEndPositions m_aStartLst; // list, each position's elements sorted by appearance order
    HTMLStartEndPositions m_aEndLst; // list, no sort of elements in position
    std::deque<sal_Int32> m_aScriptChgLst; // positions where script changes
        // 0 is not contained in this list,
        // but the text length
    // the script that is valid up to the position
    // contained in aScriptChgList at the same index
    std::vector<sal_uInt16> m_aScriptLst;

    SwDoc* m_pDoc; // the current document
    SwDoc* m_pTemplate; // the HTML template (or 0)
    std::optional<Color> m_xDefaultColor; // the default foreground colors
    std::set<UIName>& m_rScriptTextStyles;

    sal_uLong m_nHTMLMode;
    bool m_bOutStyles : 1; // are styles exported

    // Insert/remove a SttEndPos in/from the Start and End lists.
    // The end position is known.
    void InsertItem_(HTMLStartEndPos* pPos);

    // determine the 'type' of the attribute
    HTMLOnOffState GetHTMLItemState( const SfxPoolItem& rItem );

    // does a specific OnTag item exist
    bool ExistsOnTagItem( sal_uInt16 nWhich, sal_Int32 nPos );

    // does an item exist that can be used to disable an attribute that
    // is exported the same way as the supplied item in the same range?
    bool ExistsOffTagItem( sal_uInt16 nWhich, sal_Int32 nStartPos,
                                          sal_Int32 nEndPos );

    // adapt the end of a split item
    void FixSplittedItem(HTMLStartEndPos* pPos, sal_Int32 nNewEnd);

    // insert an attribute in the lists and, if necessary, split it
    void InsertItem( const SfxPoolItem& rItem, sal_Int32 nStart,
                                               sal_Int32 nEnd );

    // split an already existing attribute
    void SplitItem( const SfxPoolItem& rItem, sal_Int32 nStart,
                                              sal_Int32 nEnd );

    // Insert without taking care of script
    void InsertNoScript( const SfxPoolItem& rItem, sal_Int32 nStart,
                          sal_Int32 nEnd, SwHTMLFormatInfos& rFormatInfos,
                         bool bParaAttrs );

    const SwHTMLFormatInfo *GetFormatInfo( const SwFormat& rFormat,
                                     SwHTMLFormatInfos& rFormatInfos );

    void OutEndAttrs(SwHTMLWriter& rWrt, std::vector<HTMLStartEndPos*>& posItems);

public:

    HTMLEndPosLst( SwDoc *pDoc, SwDoc* pTemplate, std::optional<Color> xDfltColor,
                   bool bOutStyles, sal_uLong nHTMLMode,
                   const OUString& rText, std::set<UIName>& rStyles );
    ~HTMLEndPosLst();

    // insert an attribute
    void Insert( const SfxPoolItem& rItem, sal_Int32 nStart,  sal_Int32 nEnd,
                 SwHTMLFormatInfos& rFormatInfos, bool bParaAttrs=false );
    void Insert( const SfxItemSet& rItemSet, sal_Int32 nStart, sal_Int32 nEnd,
                 SwHTMLFormatInfos& rFormatInfos, bool bDeep,
                 bool bParaAttrs=false );
    void Insert( const SwDrawFrameFormat& rFormat, sal_Int32 nPos,
                 SwHTMLFormatInfos& rFormatInfos );

    sal_uInt16 GetScriptAtPos( sal_Int32 nPos,
                               sal_uInt16 nWeak );

    void OutStartAttrs( SwHTMLWriter& rWrt, sal_Int32 nPos );
    void OutEndAttrs( SwHTMLWriter& rWrt, sal_Int32 nPos );

    bool IsHTMLMode(sal_uLong nMode) const { return (m_nHTMLMode & nMode) != 0; }
};

struct SortEnds
{
    HTMLStartEndPositions& m_startList;
    SortEnds(HTMLStartEndPositions& startList) : m_startList(startList) {}
    bool operator()(const HTMLStartEndPos* p1, const HTMLStartEndPos* p2)
    {
        // if p1 start after p2, then it ends before
        if (p1->GetStart() > p2->GetStart())
            return true;
        if (p1->GetStart() < p2->GetStart())
            return false;
        for (const auto p : m_startList[p1->GetStart()])
        {
            if (p == p1)
                return false;
            if (p == p2)
                return true;
        }
        assert(!"Neither p1 nor p2 found in their start list");
        return false;
    }
};

#ifndef NDEBUG
bool IsEmpty(const HTMLStartEndPositions& l)
{
    return std::find_if(l.begin(), l.end(), [](auto& i) { return !i.second.empty(); }) == l.end();
}
#endif

}

void HTMLEndPosLst::InsertItem_(HTMLStartEndPos* pPos)
{
    // Character border attribute must be the first which is written out because of border merge.
    auto& posItems1 = m_aStartLst[pPos->GetStart()];
    auto it = pPos->GetItem().Which() == RES_CHRATR_BOX ? posItems1.begin() : posItems1.end();
    posItems1.insert(it, pPos);

    m_aEndLst[pPos->GetEnd()].push_back(pPos);
}

HTMLOnOffState HTMLEndPosLst::GetHTMLItemState( const SfxPoolItem& rItem )
{
    HTMLOnOffState eState = HTML_NOT_SUPPORTED;
    switch( rItem.Which() )
    {
    case RES_CHRATR_POSTURE:
    case RES_CHRATR_CJK_POSTURE:
    case RES_CHRATR_CTL_POSTURE:
        switch( static_cast<const SvxPostureItem&>(rItem).GetPosture() )
        {
        case ITALIC_NORMAL:
            eState = HTML_ON_VALUE;
            break;
        case ITALIC_NONE:
            eState = HTML_OFF_VALUE;
            break;
        default:
            if( IsHTMLMode(HTMLMODE_SOME_STYLES) )
                eState = HTML_STYLE_VALUE;
            break;
        }
        break;

    case RES_CHRATR_CROSSEDOUT:
        switch( rItem.StaticWhichCast(RES_CHRATR_CROSSEDOUT).GetStrikeout() )
        {
        case STRIKEOUT_SINGLE:
        case STRIKEOUT_DOUBLE:
            eState = HTML_ON_VALUE;
            break;
        case STRIKEOUT_NONE:
            eState = HTML_OFF_VALUE;
            break;
        default:
            ;
        }
        break;

    case RES_CHRATR_ESCAPEMENT:
        switch (rItem.StaticWhichCast(RES_CHRATR_ESCAPEMENT).GetEscapement())
        {
        case SvxEscapement::Superscript:
        case SvxEscapement::Subscript:
            eState = HTML_ON_VALUE;
            break;
        case SvxEscapement::Off:
            eState = HTML_OFF_VALUE;
            break;
        default:
            ;
        }
        break;

    case RES_CHRATR_UNDERLINE:
        switch( rItem.StaticWhichCast(RES_CHRATR_UNDERLINE).GetLineStyle() )
        {
        case LINESTYLE_SINGLE:
            eState = HTML_ON_VALUE;
            break;
        case LINESTYLE_NONE:
            eState = HTML_OFF_VALUE;
            break;
        default:
            if( IsHTMLMode(HTMLMODE_SOME_STYLES) )
                eState = HTML_STYLE_VALUE;
            break;
        }
        break;

    case RES_CHRATR_OVERLINE:
    case RES_CHRATR_HIDDEN:
        if( IsHTMLMode(HTMLMODE_SOME_STYLES) )
            eState = HTML_STYLE_VALUE;
        break;

    case RES_CHRATR_WEIGHT:
    case RES_CHRATR_CJK_WEIGHT:
    case RES_CHRATR_CTL_WEIGHT:
        switch( static_cast<const SvxWeightItem&>(rItem).GetWeight() )
        {
        case WEIGHT_BOLD:
            eState = HTML_ON_VALUE;
            break;
        case WEIGHT_NORMAL:
            eState = HTML_OFF_VALUE;
            break;
        default:
            if( IsHTMLMode(HTMLMODE_SOME_STYLES) )
                eState = HTML_STYLE_VALUE;
            break;
        }
        break;

    case RES_CHRATR_BLINK:
        eState = rItem.StaticWhichCast(RES_CHRATR_BLINK).GetValue() ? HTML_ON_VALUE
                                                         : HTML_OFF_VALUE;
        break;

    case RES_CHRATR_COLOR:
        eState = HTML_COLOR_VALUE;
        break;

    case RES_CHRATR_FONT:
    case RES_CHRATR_FONTSIZE:
    case RES_CHRATR_LANGUAGE:
    case RES_CHRATR_CJK_FONT:
    case RES_CHRATR_CJK_FONTSIZE:
    case RES_CHRATR_CJK_LANGUAGE:
    case RES_CHRATR_CTL_FONT:
    case RES_CHRATR_CTL_FONTSIZE:
    case RES_CHRATR_CTL_LANGUAGE:
    case RES_TXTATR_INETFMT:
        eState = HTML_REAL_VALUE;
        break;

    case RES_TXTATR_CHARFMT:
        eState = HTML_CHRFMT_VALUE;
        break;

    case RES_TXTATR_AUTOFMT:
        eState = HTML_AUTOFMT_VALUE;
        break;

    case RES_CHRATR_CASEMAP:
        eState = HTML_STYLE_VALUE;
        break;

    case RES_CHRATR_KERNING:
        eState = HTML_STYLE_VALUE;
        break;

    case RES_CHRATR_BACKGROUND:
        if( IsHTMLMode(HTMLMODE_SOME_STYLES) )
            eState = HTML_STYLE_VALUE;
        break;

    case RES_PARATR_DROP:
        eState = HTML_DROPCAP_VALUE;
        break;

    case RES_CHRATR_BOX:
        if( IsHTMLMode(HTMLMODE_SOME_STYLES) )
            eState = HTML_STYLE_VALUE;
        break;
    }

    return eState;
}

bool HTMLEndPosLst::ExistsOnTagItem( sal_uInt16 nWhich, sal_Int32 nPos )
{
    for (const auto& [startPos, items] : m_aStartLst)
    {
        if (startPos > nPos)
        {
            // this attribute, and all attributes that follow, start later
            break;
        }

        for (const auto* pTest : items)
        {
            if (pTest->GetEnd() > nPos)
            {
                // the attribute starts before, or at, the current position and ends after it
                const SfxPoolItem& rItem = pTest->GetItem();
                if (rItem.Which() == nWhich && HTML_ON_VALUE == GetHTMLItemState(rItem))
                {
                    // an OnTag attribute was found
                    return true;
                }
            }
        }
    }

    return false;
}

bool HTMLEndPosLst::ExistsOffTagItem( sal_uInt16 nWhich, sal_Int32 nStartPos,
                                      sal_Int32 nEndPos )
{
    if( nWhich != RES_CHRATR_CROSSEDOUT &&
        nWhich != RES_CHRATR_UNDERLINE &&
        nWhich != RES_CHRATR_BLINK )
    {
        return false;
    }

    for (const auto* pTest : m_aStartLst[nStartPos])
    {
        if (pTest->GetEnd() == nEndPos)
        {
            // the attribute starts before or at the current position and ends after it
            const SfxPoolItem& rItem = pTest->GetItem();
            sal_uInt16 nTstWhich = rItem.Which();
            if( (nTstWhich == RES_CHRATR_CROSSEDOUT ||
                 nTstWhich == RES_CHRATR_UNDERLINE ||
                 nTstWhich == RES_CHRATR_BLINK) &&
                HTML_OFF_VALUE == GetHTMLItemState(rItem) )
            {
                // an OffTag attribute was found that is exported the same
                // way as the current item
                return true;
            }
        }
    }

    return false;
}

void HTMLEndPosLst::FixSplittedItem(HTMLStartEndPos* pPos, sal_Int32 nNewEnd)
{
    // remove the item from the End list
    std::erase(m_aEndLst[pPos->GetEnd()], pPos);
    // fix the end position accordingly
    pPos->SetEnd( nNewEnd );
    // from now on, it is closed at the corresponding position
    m_aEndLst[nNewEnd].push_back(pPos);

    // now, adjust the attributes that got started afterwards
    const sal_Int32 nPos = pPos->GetStart();
    for (const auto& [startPos, items] : m_aStartLst)
    {
        if (startPos < nPos)
            continue;

        if (startPos >= nNewEnd)
            break;

        auto it = items.begin();
        if (startPos == nPos)
        {
            it = std::find(items.begin(), items.end(), pPos);
            if (it != items.end())
                ++it;
        }
        for (; it != items.end(); ++it)
        {
            HTMLStartEndPos* pTest = *it;
            const sal_Int32 nTestEnd = pTest->GetEnd();
            if (nTestEnd <= nNewEnd)
                continue;

            // the Test attribute starts before the split attribute
            // ends, and ends afterwards, i.e., it must be split, as well

            // remove the attribute from the End list
            std::erase(m_aEndLst[pTest->GetEnd()], pTest);
            // set the new end
            pTest->SetEnd( nNewEnd );
            // it now ends in the respective position.
            m_aEndLst[nNewEnd].push_back(pTest);

            // insert the 'rest' of the attribute
            InsertItem( pTest->GetItem(), nNewEnd, nTestEnd );
        }
    }
}

void HTMLEndPosLst::InsertItem( const SfxPoolItem& rItem, sal_Int32 nStart,
                                                          sal_Int32 nEnd )
{
    assert(nStart < nEnd);

    for (auto& [endPos, items] : m_aEndLst)
    {
        if (endPos <= nStart)
        {
            // the Test attribute ends, before the new one starts
            continue;
        }
        if (endPos >= nEnd)
        {
            // the Test attribute (and all that follow) ends, before the new
            // one ends
            break;
        }

        std::sort(items.begin(), items.end(), SortEnds(m_aStartLst));
        // Iterate over a temporary copy of items, because InsertItem_ may modify the vector
        for (HTMLStartEndPos* pTest : std::vector(items))
        {
            if( pTest->GetStart() < nStart )
            {
                // the Test attribute ends, before the new one ends. Thus, the
                // new attribute must be split.
                InsertItem_(new HTMLStartEndPos(rItem, nStart, endPos));
                nStart = endPos;
            }
        }
    }

    // one attribute must still be inserted
    InsertItem_(new HTMLStartEndPos(rItem, nStart, nEnd));
}

void HTMLEndPosLst::SplitItem( const SfxPoolItem& rItem, sal_Int32 nStart,
                                                           sal_Int32 nEnd )
{
    sal_uInt16 nWhich = rItem.Which();

    // first, we must search for the old items by using the start list and
    // determine the new item range

    for (auto& [nTestStart, items] : m_aStartLst)
    {
        if( nTestStart >= nEnd )
        {
            // this attribute, and all that follow, start later
            break;
        }

        for (auto it = items.begin(); it != items.end();)
        {
            auto itTest = it++; // forward early, allow 'continue', and keep a copy for 'erase'
            HTMLStartEndPos* pTest = *itTest;
            const sal_Int32 nTestEnd = pTest->GetEnd();
            if (nTestEnd <= nStart)
                continue;

            // the Test attribute ends in the range that must be deleted
            const SfxPoolItem& rTestItem = pTest->GetItem();

            // only the corresponding OnTag attributes have to be considered
            if (rTestItem.Which() != nWhich || HTML_ON_VALUE != GetHTMLItemState(rTestItem))
                continue;

            // if necessary, insert the second part of the split attribute
            if (nTestEnd > nEnd)
                InsertItem(rTestItem, nEnd, nTestEnd);

            if (nTestStart >= nStart)
            {
                // the Test item only starts after the new end of the
                // attribute. Therefore, it can be completely erased.
                it = items.erase(itTest);
                std::erase(m_aEndLst[nTestEnd], pTest);
                delete pTest;
                continue;
            }

            // the start of the new attribute corresponds to the new end of the attribute
            FixSplittedItem(pTest, nStart);
        }
    }
}

const SwHTMLFormatInfo *HTMLEndPosLst::GetFormatInfo( const SwFormat& rFormat,
                                                SwHTMLFormatInfos& rFormatInfos )
{
    SwHTMLFormatInfo *pFormatInfo;
    std::unique_ptr<SwHTMLFormatInfo> pTmpInfo(new SwHTMLFormatInfo(&rFormat));
    SwHTMLFormatInfos::iterator it = rFormatInfos.find( pTmpInfo );
    if (it != rFormatInfos.end())
    {
        pFormatInfo = it->get();
    }
    else
    {
        pFormatInfo = new SwHTMLFormatInfo(&rFormat, m_pDoc, m_pTemplate, m_bOutStyles);
        rFormatInfos.insert(std::unique_ptr<SwHTMLFormatInfo>(pFormatInfo));
        if (m_rScriptTextStyles.count(rFormat.GetName()))
            pFormatInfo->bScriptDependent = true;
    }

    return pFormatInfo;
}

HTMLEndPosLst::HTMLEndPosLst(SwDoc* pD, SwDoc* pTempl, std::optional<Color> xDfltCol, bool bStyles,
                             sal_uLong nMode, const OUString& rText, std::set<UIName>& rStyles)
    : m_pDoc(pD)
    , m_pTemplate(pTempl)
    , m_xDefaultColor(std::move(xDfltCol))
    , m_rScriptTextStyles(rStyles)
    , m_nHTMLMode(nMode)
    , m_bOutStyles(bStyles)
{
    sal_Int32 nEndPos = rText.getLength();
    sal_Int32 nPos = 0;
    while( nPos < nEndPos )
    {
        sal_uInt16 nScript = g_pBreakIt->GetBreakIter()->getScriptType( rText, nPos );
        nPos = g_pBreakIt->GetBreakIter()->endOfScript( rText, nPos, nScript );
        m_aScriptChgLst.push_back(nPos);
        m_aScriptLst.push_back(nScript);
    }
}

HTMLEndPosLst::~HTMLEndPosLst()
{
    assert(IsEmpty(m_aStartLst) && "Start List not empty in destructor");
    assert(IsEmpty(m_aEndLst) && "End List not empty in destructor");
}

void HTMLEndPosLst::InsertNoScript( const SfxPoolItem& rItem,
                            sal_Int32 nStart, sal_Int32 nEnd,
                            SwHTMLFormatInfos& rFormatInfos, bool bParaAttrs )
{
    // no range ?? in that case, don't take it, it will never take effect !!
    if( nStart == nEnd )
        return;

    bool bSet = false, bSplit = false;
    switch( GetHTMLItemState(rItem) )
    {
    case HTML_ON_VALUE:
        // output the attribute, if it isn't 'on', already
        if( !ExistsOnTagItem( rItem.Which(), nStart ) )
            bSet = true;
        break;

    case HTML_OFF_VALUE:
        // If the corresponding attribute is 'on', split it.
        // Additionally, output it as Style, if it is not set for the
        // whole paragraph, because in that case it was already output
        // together with the paragraph tag.
        if( ExistsOnTagItem( rItem.Which(), nStart ) )
            bSplit = true;
        bSet = m_bOutStyles && !bParaAttrs && !ExistsOffTagItem(rItem.Which(), nStart, nEnd);
        break;

    case HTML_REAL_VALUE:
        // we can always output the attribute
        bSet = true;
        break;

    case HTML_STYLE_VALUE:
        // We can only output the attribute as CSS1. If it is set for
        // the paragraph, it was already output with the paragraph tag.
        // The only exception is the character-background attribute. This
        // attribute must always be handled like a Hint.
        bSet = m_bOutStyles
               && (!bParaAttrs || rItem.Which() == RES_CHRATR_BACKGROUND
                   || rItem.Which() == RES_CHRATR_BOX || rItem.Which() == RES_CHRATR_OVERLINE);
        break;

    case HTML_CHRFMT_VALUE:
        {
            OSL_ENSURE( RES_TXTATR_CHARFMT == rItem.Which(),
                    "Not a character style after all" );
            const SwFormatCharFormat& rChrFormat = rItem.StaticWhichCast(RES_TXTATR_CHARFMT);
            const SwCharFormat* pFormat = rChrFormat.GetCharFormat();

            const SwHTMLFormatInfo *pFormatInfo = GetFormatInfo( *pFormat, rFormatInfos );
            if( !pFormatInfo->aToken.isEmpty() )
            {
                // output the character style tag before the hard
                // attributes
                InsertItem( rItem, nStart, nEnd );
            }
            if( pFormatInfo->moItemSet )
            {
                Insert( *pFormatInfo->moItemSet, nStart, nEnd,
                        rFormatInfos, true, bParaAttrs );
            }
        }
        break;

    case HTML_AUTOFMT_VALUE:
        {
            OSL_ENSURE( RES_TXTATR_AUTOFMT == rItem.Which(),
                    "Not an automatic style, after all" );
            const SwFormatAutoFormat& rAutoFormat = rItem.StaticWhichCast(RES_TXTATR_AUTOFMT);
            const std::shared_ptr<SfxItemSet>& pSet = rAutoFormat.GetStyleHandle();
            if( pSet )
                Insert( *pSet, nStart, nEnd, rFormatInfos, true, bParaAttrs );
        }
        break;

    case HTML_COLOR_VALUE:
        // A foreground color as a paragraph attribute is only exported if
        // it is not the same as the default color.
        {
            OSL_ENSURE( RES_CHRATR_COLOR == rItem.Which(),
                    "Not a foreground color, after all" );
            Color aColor( rItem.StaticWhichCast(RES_CHRATR_COLOR).GetValue() );
            if( COL_AUTO == aColor )
                aColor = COL_BLACK;
            bSet = !bParaAttrs || !m_xDefaultColor || !m_xDefaultColor->IsRGBEqual(aColor);
        }
        break;

    case HTML_DROPCAP_VALUE:
        {
            OSL_ENSURE( RES_PARATR_DROP == rItem.Which(),
                    "Not a drop cap, after all" );
            const SwFormatDrop& rDrop = rItem.StaticWhichCast(RES_PARATR_DROP);
            nEnd = nStart + rDrop.GetChars();
            if (!m_bOutStyles)
            {
                // At least use the attributes of the character style
                const SwCharFormat *pCharFormat = rDrop.GetCharFormat();
                if( pCharFormat )
                {
                    Insert( pCharFormat->GetAttrSet(), nStart, nEnd,
                            rFormatInfos, true, bParaAttrs );
                }
            }
            else
            {
                bSet = true;
            }
        }
        break;
    default:
        ;
    }

    if( bSet )
        InsertItem( rItem, nStart, nEnd );
    if( bSplit )
        SplitItem( rItem, nStart, nEnd );
}

void HTMLEndPosLst::Insert( const SfxPoolItem& rItem,
                            sal_Int32 nStart, sal_Int32 nEnd,
                            SwHTMLFormatInfos& rFormatInfos, bool bParaAttrs )
{
    bool bDependsOnScript = false, bDependsOnAnyScript = false;
    sal_uInt16 nScript = i18n::ScriptType::LATIN;
    switch( rItem.Which() )
    {
    case RES_CHRATR_FONT:
    case RES_CHRATR_FONTSIZE:
    case RES_CHRATR_LANGUAGE:
    case RES_CHRATR_POSTURE:
    case RES_CHRATR_WEIGHT:
        bDependsOnScript = true;
        nScript = i18n::ScriptType::LATIN;
        break;

    case RES_CHRATR_CJK_FONT:
    case RES_CHRATR_CJK_FONTSIZE:
    case RES_CHRATR_CJK_LANGUAGE:
    case RES_CHRATR_CJK_POSTURE:
    case RES_CHRATR_CJK_WEIGHT:
        bDependsOnScript = true;
        nScript = i18n::ScriptType::ASIAN;
        break;

    case RES_CHRATR_CTL_FONT:
    case RES_CHRATR_CTL_FONTSIZE:
    case RES_CHRATR_CTL_LANGUAGE:
    case RES_CHRATR_CTL_POSTURE:
    case RES_CHRATR_CTL_WEIGHT:
        bDependsOnScript = true;
        nScript = i18n::ScriptType::COMPLEX;
        break;
    case RES_TXTATR_CHARFMT:
        {
            const SwFormatCharFormat& rChrFormat = rItem.StaticWhichCast(RES_TXTATR_CHARFMT);
            const SwCharFormat* pFormat = rChrFormat.GetCharFormat();
            const SwHTMLFormatInfo *pFormatInfo = GetFormatInfo( *pFormat, rFormatInfos );
            if( pFormatInfo->bScriptDependent )
            {
                bDependsOnScript = true;
                bDependsOnAnyScript = true;
            }
        }
        break;
    case RES_TXTATR_INETFMT:
        {
            if (GetFormatInfo(*m_pDoc->getIDocumentStylePoolAccess().GetCharFormatFromPool(
                                  RES_POOLCHR_INET_NORMAL),
                              rFormatInfos)
                    ->bScriptDependent
                || GetFormatInfo(*m_pDoc->getIDocumentStylePoolAccess().GetCharFormatFromPool(
                                     RES_POOLCHR_INET_VISIT),
                                 rFormatInfos)
                       ->bScriptDependent)
            {
                bDependsOnScript = true;
                bDependsOnAnyScript = true;
            }
        }
        break;
    }

    if( bDependsOnScript )
    {
        sal_Int32 nPos = nStart;
        for (size_t i = 0; i < m_aScriptChgLst.size(); i++)
        {
            sal_Int32 nChgPos = m_aScriptChgLst[i];
            if( nPos >= nChgPos )
            {
                // the hint starts behind or at the next script change,
                // so we may continue with this position.
                continue;
            }
            if( nEnd <= nChgPos )
            {
                // the (rest of) the hint ends before or at the next script
                // change, so we can insert it, but only if it belongs
                // to the current script.
                if (bDependsOnAnyScript || nScript == m_aScriptLst[i])
                    InsertNoScript( rItem, nPos, nEnd, rFormatInfos,
                                    bParaAttrs );
                break;
            }

            // the hint starts before the next script change and ends behind
            // it, so we can insert a hint up to the next script change and
            // continue with the rest of the hint.
            if (bDependsOnAnyScript || nScript == m_aScriptLst[i])
                InsertNoScript( rItem, nPos, nChgPos, rFormatInfos, bParaAttrs );
            nPos = nChgPos;
        }
    }
    else
    {
        InsertNoScript( rItem, nStart, nEnd, rFormatInfos, bParaAttrs );
    }
}

void HTMLEndPosLst::Insert( const SfxItemSet& rItemSet,
                            sal_Int32 nStart, sal_Int32 nEnd,
                            SwHTMLFormatInfos& rFormatInfos,
                            bool bDeep, bool bParaAttrs )
{
    SfxWhichIter aIter( rItemSet );

    sal_uInt16 nWhich = aIter.FirstWhich();
    while( nWhich )
    {
        const SfxPoolItem *pItem;
        if( SfxItemState::SET == aIter.GetItemState( bDeep, &pItem ) )
        {
            Insert( *pItem, nStart, nEnd, rFormatInfos, bParaAttrs );
        }

        nWhich = aIter.NextWhich();
    }
}

void HTMLEndPosLst::Insert( const SwDrawFrameFormat& rFormat, sal_Int32 nPos,
                            SwHTMLFormatInfos& rFormatInfos )
{
    const SdrObject* pTextObj = SwHTMLWriter::GetMarqueeTextObj( rFormat );

    if( !pTextObj )
        return;

    // get the edit engine attributes of the object as SW attributes and
    // insert them as hints. Because of the amount of Hints the styles
    // are not considered!
    const SfxItemSet& rFormatItemSet = rFormat.GetAttrSet();
    SfxItemSetFixed<RES_CHRATR_BEGIN, RES_CHRATR_END> aItemSet( *rFormatItemSet.GetPool() );
    SwHTMLWriter::GetEEAttrsFromDrwObj( aItemSet, pTextObj );
    bool bOutStylesOld = m_bOutStyles;
    m_bOutStyles = false;
    Insert( aItemSet, nPos, nPos+1, rFormatInfos, false );
    m_bOutStyles = bOutStylesOld;
}

sal_uInt16 HTMLEndPosLst::GetScriptAtPos( sal_Int32 nPos, sal_uInt16 nWeak )
{
    sal_uInt16 nRet = CSS1_OUTMODE_ANY_SCRIPT;

    size_t nScriptChgs = m_aScriptChgLst.size();
    size_t i=0;
    while (i < nScriptChgs && nPos >= m_aScriptChgLst[i])
        i++;
    OSL_ENSURE( i < nScriptChgs, "script list is too short" );
    if( i < nScriptChgs )
    {
        if (i18n::ScriptType::WEAK == m_aScriptLst[i])
            nRet = nWeak;
        else
            nRet = SwHTMLWriter::GetCSS1ScriptForScriptType(m_aScriptLst[i]);
    }

    return nRet;
}

void HTMLEndPosLst::OutStartAttrs( SwHTMLWriter& rWrt, sal_Int32 nPos )
{
    rWrt.m_bTagOn = true;

    auto it = m_aStartLst.find(nPos);
    if (it == m_aStartLst.end())
        return;
    // the attributes of the start list are sorted in ascending order
    for (HTMLStartEndPos* pPos : it->second)
    {
        // output the attribute
        sal_uInt16 nCSS1Script = rWrt.m_nCSS1Script;
        sal_uInt16 nWhich = pPos->GetItem().Which();
        if( RES_TXTATR_CHARFMT == nWhich ||
            RES_TXTATR_INETFMT == nWhich ||
             RES_PARATR_DROP == nWhich )
        {
            rWrt.m_nCSS1Script = GetScriptAtPos( nPos, nCSS1Script );
        }
        HTMLOutFuncs::FlushToAscii( rWrt.Strm() ); // was one time only - do we still need it?
        Out( aHTMLAttrFnTab, pPos->GetItem(), rWrt );
        rWrt.maStartedAttributes[pPos->GetItem().Which()]++;
        rWrt.m_nCSS1Script = nCSS1Script;
    }
}

void HTMLEndPosLst::OutEndAttrs( SwHTMLWriter& rWrt, sal_Int32 nPos )
{
    rWrt.m_bTagOn = false;

    if (nPos == SAL_MAX_INT32)
    {
        for (auto& element : m_aEndLst)
            OutEndAttrs(rWrt, element.second);
    }
    else
    {
        auto it = m_aEndLst.find(nPos);
        if (it != m_aEndLst.end())
            OutEndAttrs(rWrt, it->second);
    }
}

void HTMLEndPosLst::OutEndAttrs(SwHTMLWriter& rWrt, std::vector<HTMLStartEndPos*>& posItems)
{
    std::sort(posItems.begin(), posItems.end(), SortEnds(m_aStartLst));
    for (auto it = posItems.begin(); it != posItems.end(); it = posItems.erase(it))
    {
        HTMLStartEndPos* pPos = *it;
        HTMLOutFuncs::FlushToAscii( rWrt.Strm() ); // was one time only - do we still need it?
        // Skip closing span if next character span has the same border (border merge)
        bool bSkipOut = false;
        if( pPos->GetItem().Which() == RES_CHRATR_BOX )
        {
            auto& startPosItems = m_aStartLst[pPos->GetEnd()];
            for (auto it2 = startPosItems.begin(); it2 != startPosItems.end(); ++it2)
            {
                HTMLStartEndPos* pEndPos = *it2;
                if( pEndPos->GetItem().Which() == RES_CHRATR_BOX &&
                    static_cast<const SvxBoxItem&>(pEndPos->GetItem()) ==
                    static_cast<const SvxBoxItem&>(pPos->GetItem()) )
                {
                    startPosItems.erase(it2);
                    pEndPos->SetStart(pPos->GetStart());
                    auto& oldStartPosItems = m_aStartLst[pEndPos->GetStart()];
                    oldStartPosItems.insert(oldStartPosItems.begin(), pEndPos);
                    bSkipOut = true;
                    break;
                }
            }
        }
        if( !bSkipOut )
        {
            Out( aHTMLAttrFnTab, pPos->GetItem(), rWrt );
            rWrt.maStartedAttributes[pPos->GetItem().Which()]--;
        }

        std::erase(m_aStartLst[pPos->GetStart()], pPos);
        delete pPos;
    }
}

static constexpr bool IsLF(sal_Unicode ch) { return ch == '\n'; }

static constexpr bool IsWhitespaceExcludingLF(sal_Unicode ch)
{
    return ch == ' ' || ch == '\t' || ch == '\r';
}

static constexpr bool IsWhitespaceIncludingLF(sal_Unicode ch)
{
    return IsWhitespaceExcludingLF(ch) || IsLF(ch);
}

static bool NeedPreserveWhitespace(std::u16string_view str, bool xml)
{
    if (str.empty())
        return false;
    // leading / trailing spaces
    // A leading / trailing \n would turn into a leading / trailing <br/>,
    // and will not disappear, even without space preserving option
    if (IsWhitespaceExcludingLF(str.front()) || IsWhitespaceExcludingLF(str.back()))
        return true;
    for (size_t i = 0; i < str.size(); ++i)
    {
        if (xml)
        {
            // No need to consider \n, which convert to <br/>, when it's after a space
            // (but handle it *before* a space)
            if (IsWhitespaceIncludingLF(str[i]))
            {
                do
                {
                    ++i;
                    if (i == str.size())
                        return false;
                } while (IsLF(str[i]));
                if (IsWhitespaceExcludingLF(str[i]))
                    return true; // Second whitespace in a row
            }
        }
        else // html
        {
            // Only consider \n, when an adjacent space is not \n - which would be eaten
            // without a space preserving option
            if (IsWhitespaceExcludingLF(str[i]))
            {
                ++i;
                if (i == str.size())
                    return false;
                if (IsWhitespaceIncludingLF(str[i]))
                    return true; // Any whitespace after a non-LF whitespace
            }
            else if (IsLF(str[i]))
            {
                do
                {
                    ++i;
                    if (i == str.size())
                        return false;
                }
                while (IsLF(str[i]));
                if (IsWhitespaceExcludingLF(str[i]))
                    return true; // A non-LF whitespace after a LF
            }
        }
    }
    return false;
}

/* Output of the nodes*/
SwHTMLWriter& OutHTML_SwTextNode( SwHTMLWriter& rWrt, const SwContentNode& rNode )
{
    const SwTextNode * pNd = &static_cast<const SwTextNode&>(rNode);

    const OUString& rStr = pNd->GetText();
    sal_Int32 nEnd = rStr.getLength();

    // special case: empty node and HR style (horizontal rule)
    //               output a <HR>, only
    sal_uInt16 nPoolId = pNd->GetAnyFormatColl().GetPoolFormatId();

    // Handle horizontal rule <hr>
    if (!nEnd &&
        (RES_POOLCOLL_HTML_HR==nPoolId || pNd->GetAnyFormatColl().GetName() == OOO_STRING_SVTOOLS_HTML_horzrule))
    {
        // then, the paragraph-anchored graphics/OLE objects in the paragraph
        // MIB 8.7.97: We enclose the line in a <PRE>. This means that the
        // spacings are wrong, but otherwise we get an empty paragraph
        // after the <HR> which is even uglier.
        rWrt.ChangeParaToken( HtmlTokenId::NONE );

        // Output all the nodes that are anchored to a frame
        rWrt.OutFlyFrame( rNode.GetIndex(), 0, HtmlPosition::Any );

        if (rWrt.IsLFPossible())
            rWrt.OutNewLine(); // paragraph tag on a new line

        rWrt.SetLFPossible(true);

        HtmlWriter aHtml(rWrt.Strm(), rWrt.GetNamespace());
        aHtml.prettyPrint(rWrt.IsPrettyPrint());
        aHtml.start(OOO_STRING_SVTOOLS_HTML_horzrule ""_ostr);

        const SfxItemSet* pItemSet = pNd->GetpSwAttrSet();
        if( !pItemSet )
        {
            aHtml.end();
            return rWrt;
        }
        if (pItemSet->GetItemIfSet(RES_MARGIN_FIRSTLINE, false)
            || pItemSet->GetItemIfSet(RES_MARGIN_TEXTLEFT, false)
            || pItemSet->GetItemIfSet(RES_MARGIN_RIGHT, false))
        {
            SvxFirstLineIndentItem const& rFirstLine(pItemSet->Get(RES_MARGIN_FIRSTLINE));
            SvxTextLeftMarginItem const& rTextLeftMargin(pItemSet->Get(RES_MARGIN_TEXTLEFT));
            SvxRightMarginItem const& rRightMargin(pItemSet->Get(RES_MARGIN_RIGHT));
            sal_Int32 const nLeft(rTextLeftMargin.ResolveLeft(rFirstLine, /*metrics*/ {}));
            sal_Int32 const nRight(rRightMargin.ResolveRight({}));
            if( nLeft || nRight )
            {
                const SwFrameFormat& rPgFormat =
                    rWrt.m_pDoc->getIDocumentStylePoolAccess().GetPageDescFromPool
                    ( RES_POOLPAGE_HTML, false )->GetMaster();
                const SwFormatFrameSize& rSz   = rPgFormat.GetFrameSize();
                const SvxLRSpaceItem& rLR = rPgFormat.GetLRSpace();
                const SwFormatCol& rCol = rPgFormat.GetCol();

                tools::Long nPageWidth
                    = rSz.GetWidth() - rLR.ResolveLeft({}) - rLR.ResolveRight({});

                if( 1 < rCol.GetNumCols() )
                    nPageWidth /= rCol.GetNumCols();

                const SwTableNode* pTableNd = pNd->FindTableNode();
                if( pTableNd )
                {
                    const SwTableBox* pBox = pTableNd->GetTable().GetTableBox(
                                    pNd->StartOfSectionIndex() );
                    if( pBox )
                        nPageWidth = pBox->GetFrameFormat()->GetFrameSize().GetWidth();
                }

                OString sWidth = OString::number(SwHTMLWriter::ToPixel(nPageWidth - nLeft - nRight));
                aHtml.attribute(OOO_STRING_SVTOOLS_HTML_O_width, sWidth);

                if( !nLeft )
                    aHtml.attribute(OOO_STRING_SVTOOLS_HTML_O_align, OOO_STRING_SVTOOLS_HTML_AL_left);
                else if( !nRight )
                    aHtml.attribute(OOO_STRING_SVTOOLS_HTML_O_align, OOO_STRING_SVTOOLS_HTML_AL_right);
                else
                    aHtml.attribute(OOO_STRING_SVTOOLS_HTML_O_align, OOO_STRING_SVTOOLS_HTML_AL_center);
            }
        }

        if( const SvxBoxItem* pBoxItem = pItemSet->GetItemIfSet( RES_BOX, false ))
        {
            const editeng::SvxBorderLine* pBorderLine = pBoxItem->GetBottom();
            if( pBorderLine )
            {
                sal_uInt16 nWidth = pBorderLine->GetScaledWidth();
                OString sWidth = OString::number(SwHTMLWriter::ToPixel(nWidth));
                aHtml.attribute(OOO_STRING_SVTOOLS_HTML_O_size, sWidth);

                const Color& rBorderColor = pBorderLine->GetColor();
                if( !rBorderColor.IsRGBEqual( COL_GRAY ) )
                {
                    HtmlWriterHelper::applyColor(aHtml, OOO_STRING_SVTOOLS_HTML_O_color, rBorderColor);
                }

                if( !pBorderLine->GetInWidth() )
                {
                    aHtml.attribute(OOO_STRING_SVTOOLS_HTML_O_noshade, OOO_STRING_SVTOOLS_HTML_O_noshade);
                }
            }
        }
        aHtml.end();
        return rWrt;
    }

    // Do not export the empty nodes with 2pt fonts and standard style that
    // are inserted before tables and sections, but do export bookmarks
    // and paragraph anchored frames.
    if( !nEnd && (nPoolId == RES_POOLCOLL_STANDARD ||
                  nPoolId == RES_POOLCOLL_TABLE ||
                  nPoolId == RES_POOLCOLL_TABLE_HDLN) )
    {
        // The current node is empty and contains the standard style ...
        const SvxFontHeightItem* pFontHeightItem;
        const SfxItemSet* pItemSet = pNd->GetpSwAttrSet();
        if( pItemSet && pItemSet->Count() &&
            (pFontHeightItem = pItemSet->GetItemIfSet( RES_CHRATR_FONTSIZE, false )) &&
            40 == pFontHeightItem->GetHeight() )
        {
            // ... moreover, the 2pt font is set ...
            SwNodeOffset nNdPos = rWrt.m_pCurrentPam->GetPoint()->GetNodeIndex();
            const SwNode *pNextNd = rWrt.m_pDoc->GetNodes()[nNdPos+1];
            const SwNode *pPrevNd = rWrt.m_pDoc->GetNodes()[nNdPos-1];
            bool bStdColl = nPoolId == RES_POOLCOLL_STANDARD;
            if( ( bStdColl && (pNextNd->IsTableNode() || pNextNd->IsSectionNode()) ) ||
                ( !bStdColl &&
                   pNextNd->IsEndNode() &&
                   pPrevNd->IsStartNode() &&
                   SwTableBoxStartNode == pPrevNd->GetStartNode()->GetStartNodeType() ) )
            {
                // ... and it is located before a table or a section
                rWrt.OutBookmarks();
                rWrt.SetLFPossible(rWrt.m_nLastParaToken == HtmlTokenId::NONE);

                // Output all frames that are anchored to this node
                rWrt.OutFlyFrame( rNode.GetIndex(), 0, HtmlPosition::Any );
                rWrt.SetLFPossible(false);

                return rWrt;
            }
        }
    }

    // catch PageBreaks and PageDescs
    bool bPageBreakBehind = false;
    if( rWrt.m_bCfgFormFeed &&
        !(rWrt.m_bOutTable || rWrt.m_bOutFlyFrame) &&
        rWrt.m_pStartNdIdx->GetIndex() != rWrt.m_pCurrentPam->GetPoint()->GetNodeIndex() )
    {
        bool bPageBreakBefore = false;
        const SfxItemSet* pItemSet = pNd->GetpSwAttrSet();

        if( pItemSet )
        {
            const SwFormatPageDesc* pPageDescItem = pItemSet->GetItemIfSet( RES_PAGEDESC );
            if( pPageDescItem && pPageDescItem->GetPageDesc() )
            {
                bPageBreakBefore = true;
            }
            else if( const SvxFormatBreakItem* pItem = pItemSet->GetItemIfSet( RES_BREAK ) )
            {
                switch( pItem->GetBreak() )
                {
                case SvxBreak::PageBefore:
                    bPageBreakBefore = true;
                    break;
                case SvxBreak::PageAfter:
                    bPageBreakBehind = true;
                    break;
                case SvxBreak::PageBoth:
                    bPageBreakBefore = true;
                    bPageBreakBehind = true;
                    break;
                default:
                    break;
                }
            }
        }

        if( bPageBreakBefore )
            rWrt.Strm().WriteChar( '\f' );
    }

    // if necessary, open a form
    rWrt.OutForm();

    // Output the page-anchored frames that are 'anchored' to this node
    bool bFlysLeft = rWrt.OutFlyFrame( rNode.GetIndex(), 0, HtmlPosition::Prefix );

    // Output all frames that are anchored to this node that are supposed to
    // be written before the paragraph tag.
    if( bFlysLeft )
    {
        bFlysLeft = rWrt.OutFlyFrame( rNode.GetIndex(), 0, HtmlPosition::Before );
    }

    if( rWrt.m_pCurrentPam->GetPoint()->GetNode() == rWrt.m_pCurrentPam->GetMark()->GetNode() )
    {
        nEnd = rWrt.m_pCurrentPam->GetMark()->GetContentIndex();
    }

    // are there any hard attributes that must be written as options?
    rWrt.m_bTagOn = true;

    // now, output the tag of the paragraph
    const SwFormat& rFormat = pNd->GetAnyFormatColl();
    SwHTMLTextCollOutputInfo aFormatInfo;
    bool bOldLFPossible = rWrt.IsLFPossible();
    bool bOldSpacePreserve = rWrt.IsSpacePreserve();
    if (rWrt.IsPreserveSpacesOnWritePrefSet())
        rWrt.SetSpacePreserve(NeedPreserveWhitespace(rStr, rWrt.mbReqIF));
    OutHTML_SwFormat( rWrt, rFormat, pNd->GetpSwAttrSet(), aFormatInfo );

    // If we didn't open a new line before the paragraph tag, we do that now
    rWrt.SetLFPossible(rWrt.m_nLastParaToken == HtmlTokenId::NONE);
    if (!bOldLFPossible && rWrt.IsLFPossible())
        rWrt.OutNewLine();

    // then, the bookmarks (including end tag)
    rWrt.m_bOutOpts = false;
    rWrt.OutBookmarks();

    // now it's a good opportunity again for an LF - if it is still allowed
    // FIXME: for LOK case we set rWrt.m_nWishLineLen as -1, for now keep old flow
    // when LOK side will be fixed - don't insert new line at the beginning
    if( rWrt.IsLFPossible() && rWrt.IsPrettyPrint() && rWrt.m_nWishLineLen >= 0 &&
        rWrt.GetLineLen() >= rWrt.m_nWishLineLen )
    {
        rWrt.OutNewLine();
    }
    rWrt.SetLFPossible(false);

    // find text that originates from an outline numbering
    sal_Int32 nOffset = 0;
    OUString aOutlineText;
    OUString aFullText;

    // export numbering string as plain text only for the outline numbering,
    // because the outline numbering isn't exported as a numbering - see <SwHTMLNumRuleInfo::Set(..)>
    if ( pNd->IsOutline() &&
         pNd->GetNumRule() == pNd->GetDoc().GetOutlineNumRule() )
    {
        aOutlineText = pNd->GetNumString();
        nOffset = nOffset + aOutlineText.getLength();
        aFullText = aOutlineText;
    }
    OUString aFootEndNoteSym;
    if( rWrt.m_pFormatFootnote )
    {
        aFootEndNoteSym = rWrt.GetFootEndNoteSym( *rWrt.m_pFormatFootnote );
        nOffset = nOffset + aFootEndNoteSym.getLength();
        aFullText += aFootEndNoteSym;
    }

    // Table of Contents or other paragraph with dot leaders?
    sal_Int32 nIndexTab = rWrt.indexOfDotLeaders( nPoolId, rStr );
    if (nIndexTab > -1)
        // skip part after the tabulator (page number)
        nEnd = nIndexTab;

    // are there any hard attributes that must be written as tags?
    aFullText += rStr;
    HTMLEndPosLst aEndPosLst( rWrt.m_pDoc, rWrt.m_xTemplate.get(),
                              rWrt.m_xDfltColor, rWrt.m_bCfgOutStyles,
                              rWrt.GetHTMLMode(), aFullText,
                                 rWrt.m_aScriptTextStyles );
    if( aFormatInfo.moItemSet )
    {
        aEndPosLst.Insert( *aFormatInfo.moItemSet, 0, nEnd + nOffset,
                           rWrt.m_CharFormatInfos, false, true );
    }

    if( !aOutlineText.isEmpty() || rWrt.m_pFormatFootnote )
    {
        // output paragraph attributes, so that the text gets the attributes of
        // the paragraph.
        aEndPosLst.OutStartAttrs( rWrt, 0 );

        // Theoretically, we would have to consider the character style of
        // the numbering. Because it cannot be set via the UI, let's ignore
        // it for now.

        if( !aOutlineText.isEmpty() )
            HTMLOutFuncs::Out_String( rWrt.Strm(), aOutlineText );

        if( rWrt.m_pFormatFootnote )
        {
            rWrt.OutFootEndNoteSym( *rWrt.m_pFormatFootnote, aFootEndNoteSym,
                                        aEndPosLst.GetScriptAtPos( aOutlineText.getLength(), rWrt.m_nCSS1Script ) );
            rWrt.m_pFormatFootnote = nullptr;
        }
    }

    // for now, correct the start. I.e., if we only output part of the sentence,
    // the attributes must be correct there, as well!!
    rWrt.m_bTextAttr = true;

    size_t nAttrPos = 0;
    sal_Int32 nStrPos = rWrt.m_pCurrentPam->GetPoint()->GetContentIndex();
    const SwTextAttr * pHt = nullptr;
    const size_t nCntAttr = pNd->HasHints() ? pNd->GetSwpHints().Count() : 0;
    if( nCntAttr && nStrPos > ( pHt = pNd->GetSwpHints().Get(0) )->GetStart() )
    {
        // Ok, there are earlier attributes that we must output
        do {
            aEndPosLst.OutEndAttrs( rWrt, nStrPos + nOffset );

            nAttrPos++;
            if( pHt->Which() == RES_TXTATR_FIELD
                || pHt->Which() == RES_TXTATR_ANNOTATION )
                continue;

            if ( pHt->End() && !pHt->HasDummyChar() )
            {
                const sal_Int32 nHtEnd = *pHt->End(),
                       nHtStt = pHt->GetStart();
                if( !rWrt.m_bWriteAll && nHtEnd <= nStrPos )
                    continue;

                // don't consider empty hints at the beginning - or should we ??
                if( nHtEnd == nHtStt )
                    continue;

                // add attribute to the list
                if( rWrt.m_bWriteAll )
                    aEndPosLst.Insert( pHt->GetAttr(), nHtStt + nOffset,
                                       nHtEnd + nOffset,
                                       rWrt.m_CharFormatInfos );
                else
                {
                    sal_Int32 nTmpStt = nHtStt < nStrPos ? nStrPos : nHtStt;
                    sal_Int32 nTmpEnd = std::min(nHtEnd, nEnd);
                    aEndPosLst.Insert( pHt->GetAttr(), nTmpStt + nOffset,
                                       nTmpEnd + nOffset,
                                       rWrt.m_CharFormatInfos );
                }
                continue;
                // but don't output it, that will be done later !!
            }

        } while( nAttrPos < nCntAttr && nStrPos >
            ( pHt = pNd->GetSwpHints().Get( nAttrPos ) )->GetStart() );

        // so, let's output all collected attributes from the string pos on
        aEndPosLst.OutEndAttrs( rWrt, nStrPos + nOffset );
        aEndPosLst.OutStartAttrs( rWrt, nStrPos + nOffset );
    }

    bool bWriteBreak = (HtmlTokenId::PREFORMTXT_ON != rWrt.m_nLastParaToken);
    if (bWriteBreak && (pNd->GetNumRule() || rWrt.mbReqIF))
    {
        // One line-break is exactly one <br> in the ReqIF case.
        bWriteBreak = false;
    }

    {
        // Tabs are leading till there is a non-tab since the start of the paragraph.
        bool bLeadingTab = true;
        for( ; nStrPos < nEnd; nStrPos++ )
        {
            // output the frames that are anchored to the current position
            if( bFlysLeft )
            {
                aEndPosLst.OutEndAttrs( rWrt, nStrPos + nOffset );
                bFlysLeft = rWrt.OutFlyFrame( rNode.GetIndex(),
                                                nStrPos, HtmlPosition::Inside );
            }

            bool bOutChar = true;
            const SwTextAttr * pTextHt = nullptr;
            if (nAttrPos < nCntAttr && pHt->GetStart() == nStrPos)
            {
                do {
                    if ( pHt->End() && !pHt->HasDummyChar() )
                    {
                        if( *pHt->End() != nStrPos )
                        {
                            // insert hints with end, if they don't start
                            // an empty range (hints that don't start a range
                            // are ignored)
                            aEndPosLst.Insert( pHt->GetAttr(), nStrPos + nOffset,
                                               *pHt->End() + nOffset,
                                               rWrt.m_CharFormatInfos );
                        }
                    }
                    else
                    {
                        // hints without an end are output last
                        OSL_ENSURE( !pTextHt, "Why is there already an attribute without an end?" );
                        if( rWrt.m_nTextAttrsToIgnore>0 )
                        {
                            rWrt.m_nTextAttrsToIgnore--;
                        }
                        else
                        {
                            pTextHt = pHt;
                            SwFieldIds nFieldWhich;
                            if( RES_TXTATR_FIELD != pHt->Which()
                                || ( SwFieldIds::Postit != (nFieldWhich = static_cast<const SwFormatField&>(pHt->GetAttr()).GetField()->Which())
                                     && SwFieldIds::Script != nFieldWhich ) )
                            {
                                bWriteBreak = false;
                            }
                        }
                        bOutChar = false;       // don't output 255
                    }
                } while( ++nAttrPos < nCntAttr && nStrPos ==
                    ( pHt = pNd->GetSwpHints().Get( nAttrPos ) )->GetStart() );
            }

            // Additionally, some draw formats can bring attributes
            if( pTextHt && RES_TXTATR_FLYCNT == pTextHt->Which() )
            {
                const SwFrameFormat* pFrameFormat =
                    pTextHt->GetAttr().StaticWhichCast(RES_TXTATR_FLYCNT).GetFrameFormat();

                if( RES_DRAWFRMFMT == pFrameFormat->Which() )
                    aEndPosLst.Insert( *static_cast<const SwDrawFrameFormat *>(pFrameFormat),
                                        nStrPos + nOffset,
                                        rWrt.m_CharFormatInfos );
            }

            aEndPosLst.OutEndAttrs( rWrt, nStrPos + nOffset );
            aEndPosLst.OutStartAttrs( rWrt, nStrPos + nOffset );

            if( pTextHt )
            {
                rWrt.SetLFPossible(rWrt.m_nLastParaToken == HtmlTokenId::NONE &&
                                       nStrPos > 0 &&
                                       rStr[nStrPos-1] == ' ');
                sal_uInt16 nCSS1Script = rWrt.m_nCSS1Script;
                rWrt.m_nCSS1Script = aEndPosLst.GetScriptAtPos(
                                                nStrPos + nOffset, nCSS1Script );
                HTMLOutFuncs::FlushToAscii( rWrt.Strm() );
                Out( aHTMLAttrFnTab, pTextHt->GetAttr(), rWrt );
                rWrt.m_nCSS1Script = nCSS1Script;
                rWrt.SetLFPossible(false);
            }

            if( bOutChar )
            {
                sal_uInt32 c = rStr[nStrPos];
                if( rtl::isHighSurrogate(c) && nStrPos < nEnd - 1 )
                {
                    const sal_Unicode d = rStr[nStrPos + 1];
                    if( rtl::isLowSurrogate(d) )
                    {
                        c = rtl::combineSurrogates(c, d);
                        nStrPos++;
                    }
                }

                // try to split a line after about 255 characters
                // at a space character unless in a PRE-context
                if( ' ' == c && rWrt.m_nLastParaToken == HtmlTokenId::NONE && !rWrt.IsSpacePreserve() )
                {
                    sal_Int32 nLineLen;
                    nLineLen = rWrt.GetLineLen();

                    sal_Int32 nWordLen = rStr.indexOf( ' ', nStrPos+1 );
                    if( nWordLen == -1 )
                        nWordLen = nEnd;
                    nWordLen -= nStrPos;

                    if( rWrt.IsPrettyPrint() && rWrt.m_nWishLineLen >= 0 &&
                        (nLineLen >= rWrt.m_nWishLineLen ||
                        (nLineLen+nWordLen) >= rWrt.m_nWishLineLen ) )
                    {
                        HTMLOutFuncs::FlushToAscii( rWrt.Strm() );
                        rWrt.OutNewLine();
                        bOutChar = false;
                    }
                }

                if( bOutChar )
                {
                    if( 0x0a == c )
                    {
                        HTMLOutFuncs::FlushToAscii( rWrt.Strm() );
                        HtmlWriter aHtml(rWrt.Strm(), rWrt.GetNamespace());
                        aHtml.prettyPrint(rWrt.IsPrettyPrint());
                        aHtml.single(OOO_STRING_SVTOOLS_HTML_linebreak ""_ostr);
                    }
                    else if (c == CH_TXT_ATR_FORMELEMENT)
                    {
                        // Placeholder for a single-point fieldmark.

                        SwPosition aMarkPos = *rWrt.m_pCurrentPam->GetPoint();
                        aMarkPos.AdjustContent( nStrPos - aMarkPos.GetContentIndex() );
                        rWrt.OutPointFieldmarks(aMarkPos);
                    }
                    else
                    {
                        bool bConsumed = false;
                        if (c == '\t')
                        {
                            if (bLeadingTab && rWrt.m_nLeadingTabWidth.has_value())
                            {
                                // Consume a tab if it's leading and we know the number of NBSPs to
                                // be used as a replacement.
                                for (sal_Int32 i = 0; i < *rWrt.m_nLeadingTabWidth; ++i)
                                {
                                    rWrt.Strm().WriteOString("&#160;");
                                }
                                bConsumed = true;
                            }
                        }
                        else
                        {
                            // Not a tab -> later tabs are no longer leading.
                            bLeadingTab = false;
                        }

                        if (!bConsumed)
                        {
                            HTMLOutFuncs::Out_Char(rWrt.Strm(), c);
                        }
                    }

                    if (!rWrt.mbReqIF)
                    {
                        // if a paragraph's last character is a hard line break
                        // then we need to add an extra <br>
                        // because browsers like Mozilla wouldn't add a line for the next paragraph
                        bWriteBreak = (0x0a == c) &&
                                      (HtmlTokenId::PREFORMTXT_ON != rWrt.m_nLastParaToken);
                    }
                }
            }
        }
        HTMLOutFuncs::FlushToAscii( rWrt.Strm() );
    }

    aEndPosLst.OutEndAttrs( rWrt, SAL_MAX_INT32 );

    // Output the frames that are anchored to the last position
    if( bFlysLeft )
        bFlysLeft = rWrt.OutFlyFrame( rNode.GetIndex(),
                                       nEnd, HtmlPosition::Inside );
    OSL_ENSURE( !bFlysLeft, "Not all frames were saved!" );

    rWrt.m_bTextAttr = false;

    if( bWriteBreak )
    {
        bool bEndOfCell = rWrt.m_bOutTable &&
                         rWrt.m_pCurrentPam->GetPoint()->GetNodeIndex() ==
                         rWrt.m_pCurrentPam->GetMark()->GetNodeIndex();

        if( bEndOfCell && !nEnd &&
            rWrt.IsHTMLMode(HTMLMODE_NBSP_IN_TABLES) )
        {
            // If the last paragraph of a table cell is empty and we export
            // for the MS-IE, we write a &nbsp; instead of a <BR>
            rWrt.Strm().WriteChar( '&' ).WriteOString( OOO_STRING_SVTOOLS_HTML_S_nbsp ).WriteChar( ';' );
        }
        else
        {
            HtmlWriter aHtml(rWrt.Strm(), rWrt.GetNamespace());
            aHtml.prettyPrint(rWrt.IsPrettyPrint());
            aHtml.single(OOO_STRING_SVTOOLS_HTML_linebreak ""_ostr);
            const SvxULSpaceItem& rULSpace = pNd->GetSwAttrSet().Get(RES_UL_SPACE);
            if (rULSpace.GetLower() > 0 && !bEndOfCell)
            {
                aHtml.single(OOO_STRING_SVTOOLS_HTML_linebreak ""_ostr);
            }
            rWrt.SetLFPossible(true);
        }
    }

    if( rWrt.m_bClearLeft || rWrt.m_bClearRight )
    {
        const char* pString;
        if( rWrt.m_bClearLeft )
        {
            if( rWrt.m_bClearRight )
                pString = OOO_STRING_SVTOOLS_HTML_AL_all;
            else
                pString = OOO_STRING_SVTOOLS_HTML_AL_left;
        }
        else
        {
            pString = OOO_STRING_SVTOOLS_HTML_AL_right;
        }

        HtmlWriter aHtml(rWrt.Strm(), rWrt.GetNamespace());
        aHtml.prettyPrint(rWrt.IsPrettyPrint());
        aHtml.start(OOO_STRING_SVTOOLS_HTML_linebreak ""_ostr);
        aHtml.attribute(OOO_STRING_SVTOOLS_HTML_O_clear, pString);
        aHtml.end();

        rWrt.m_bClearLeft = false;
        rWrt.m_bClearRight = false;

        rWrt.SetLFPossible(true);
    }

    // if an LF is not allowed already, it is allowed once the paragraphs
    // ends with a ' '
    if (!rWrt.IsLFPossible() &&
        rWrt.m_nLastParaToken == HtmlTokenId::NONE &&
        nEnd > 0 && ' ' == rStr[nEnd-1] )
        rWrt.SetLFPossible(true);

    // dot leaders: print the skipped page number in a different span element
    if (nIndexTab > -1) {
        OString sOut = OUStringToOString(rStr.subView(nIndexTab + 1), RTL_TEXTENCODING_ASCII_US);
        rWrt.Strm().WriteOString( Concat2View("</span><span>" + sOut + "</span>") );
    }

    rWrt.m_bTagOn = false;
    OutHTML_SwFormatOff( rWrt, aFormatInfo );
    rWrt.SetSpacePreserve(bOldSpacePreserve);

    // if necessary, close a form
    rWrt.OutForm( false );

    if( bPageBreakBehind )
        rWrt.Strm().WriteChar( '\f' );

    return rWrt;
}

// In CSS, "px" is 1/96 of inch: https://www.w3.org/TR/css3-values/#absolute-lengths
sal_uInt32 SwHTMLWriter::ToPixel(sal_uInt32 nTwips)
{
    // if there is a Twip, there should be a pixel as well
    return nTwips
               ? std::max(o3tl::convert(nTwips, o3tl::Length::twip, o3tl::Length::px), sal_Int64(1))
               : 0;
}

Size SwHTMLWriter::ToPixel(Size aTwips)
{
    return Size(ToPixel(aTwips.Width()), ToPixel(aTwips.Height()));
}

static SwHTMLWriter& OutHTML_CSS1Attr( SwHTMLWriter& rWrt, const SfxPoolItem& rHt )
{
    // if hints are currently written, we try to write the hint as an
    // CSS1 attribute

    if( rWrt.m_bCfgOutStyles && rWrt.m_bTextAttr )
        OutCSS1_HintSpanTag( rWrt, rHt );

    return rWrt;
}

/* File CHRATR.HXX: */

static SwHTMLWriter& OutHTML_SvxColor( SwHTMLWriter& rWrt, const SfxPoolItem& rHt )
{
    if( rWrt.m_bOutOpts )
        return rWrt;

    if( !rWrt.m_bTextAttr && rWrt.m_bCfgOutStyles && rWrt.m_bCfgPreferStyles )
    {
        // don't write the font color as a tag, if styles are preferred to
        // normal tags
        return rWrt;
    }

    if( rWrt.m_bTagOn )
    {
        Color aColor( static_cast<const SvxColorItem&>(rHt).GetValue() );
        if( COL_AUTO == aColor )
            aColor = COL_BLACK;

        if (rWrt.mbXHTML)
        {
            OString sOut = "<" + rWrt.GetNamespace() + OOO_STRING_SVTOOLS_HTML_span
                           " " OOO_STRING_SVTOOLS_HTML_O_style "=";
            rWrt.Strm().WriteOString(sOut);
            HTMLOutFuncs::Out_Color(rWrt.Strm(), aColor, /*bXHTML=*/true).WriteChar('>');
        }
        else
        {
            OString sOut = "<" + rWrt.GetNamespace() + OOO_STRING_SVTOOLS_HTML_font " "
                OOO_STRING_SVTOOLS_HTML_O_color "=";
            rWrt.Strm().WriteOString( sOut );
            HTMLOutFuncs::Out_Color( rWrt.Strm(), aColor ).WriteChar( '>' );
        }
    }
    else
    {
        if (rWrt.mbXHTML)
            HTMLOutFuncs::Out_AsciiTag(
                rWrt.Strm(), Concat2View(rWrt.GetNamespace() + OOO_STRING_SVTOOLS_HTML_span), false);
        else
            HTMLOutFuncs::Out_AsciiTag( rWrt.Strm(), Concat2View(rWrt.GetNamespace() + OOO_STRING_SVTOOLS_HTML_font), false );
    }

    return rWrt;
}

static SwHTMLWriter& OutHTML_SwPosture( SwHTMLWriter& rWrt, const SfxPoolItem& rHt )
{
    if( rWrt.m_bOutOpts )
        return rWrt;

    const FontItalic nPosture = static_cast<const SvxPostureItem&>(rHt).GetPosture();
    if( ITALIC_NORMAL == nPosture )
    {
        HTMLOutFuncs::Out_AsciiTag( rWrt.Strm(), Concat2View(rWrt.GetNamespace() + OOO_STRING_SVTOOLS_HTML_italic), rWrt.m_bTagOn );
    }
    else if( rWrt.m_bCfgOutStyles && rWrt.m_bTextAttr )
    {
        // maybe as CSS1 attribute?
        OutCSS1_HintSpanTag( rWrt, rHt );
    }

    return rWrt;
}

static SwHTMLWriter& OutHTML_SvxFont( SwHTMLWriter& rWrt, const SfxPoolItem& rHt )
{
    if( rWrt.m_bOutOpts )
        return rWrt;

    if (IgnorePropertyForReqIF(rWrt.mbReqIF, "font-family", ""))
    {
        return rWrt;
    }

    if( rWrt.m_bTagOn )
    {
        OUString aNames;
        SwHTMLWriter::PrepareFontList( static_cast<const SvxFontItem&>(rHt), aNames, 0,
                           rWrt.IsHTMLMode(HTMLMODE_FONT_GENERIC) );
        if (rWrt.mbXHTML)
        {
            OString sOut = "<" + rWrt.GetNamespace() + OOO_STRING_SVTOOLS_HTML_span
                           " " OOO_STRING_SVTOOLS_HTML_O_style "=\"font-family: ";
            rWrt.Strm().WriteOString(sOut);
            HTMLOutFuncs::Out_String(rWrt.Strm(), aNames)
                .WriteOString("\">");
        }
        else
        {
            OString sOut = "<" + rWrt.GetNamespace() + OOO_STRING_SVTOOLS_HTML_font " "
                OOO_STRING_SVTOOLS_HTML_O_face "=\"";
            rWrt.Strm().WriteOString( sOut );
            HTMLOutFuncs::Out_String( rWrt.Strm(), aNames )
               .WriteOString( "\">" );
        }
    }
    else
    {
        if (rWrt.mbXHTML)
            HTMLOutFuncs::Out_AsciiTag(
                rWrt.Strm(), Concat2View(rWrt.GetNamespace() + OOO_STRING_SVTOOLS_HTML_span), false);
        else
            HTMLOutFuncs::Out_AsciiTag( rWrt.Strm(), Concat2View(rWrt.GetNamespace() + OOO_STRING_SVTOOLS_HTML_font), false );
    }

    return rWrt;
}

static SwHTMLWriter& OutHTML_SvxFontHeight( SwHTMLWriter& rWrt, const SfxPoolItem& rHt )
{
    if( rWrt.m_bOutOpts )
        return rWrt;

    if (IgnorePropertyForReqIF(rWrt.mbReqIF, "font-size", ""))
    {
        return rWrt;
    }

    if( rWrt.m_bTagOn )
    {
        if (rWrt.mbXHTML)
        {
            OString sOut = "<" + rWrt.GetNamespace() + OOO_STRING_SVTOOLS_HTML_span;

            sal_uInt32 nHeight = static_cast<const SvxFontHeightItem&>(rHt).GetHeight();
            // Twips -> points.
            sal_uInt16 nSize = nHeight / 20;
            sOut += " " OOO_STRING_SVTOOLS_HTML_O_style "=\"font-size: "
                    + OString::number(static_cast<sal_Int32>(nSize)) + "pt\"";
            rWrt.Strm().WriteOString(sOut);
        }
        else
        {
            OString sOut = "<" + rWrt.GetNamespace() + OOO_STRING_SVTOOLS_HTML_font;

            sal_uInt32 nHeight = static_cast<const SvxFontHeightItem&>(rHt).GetHeight();
            sal_uInt16 nSize = rWrt.GetHTMLFontSize( nHeight );
            sOut += " " OOO_STRING_SVTOOLS_HTML_O_size "=\"" +
                OString::number(static_cast<sal_Int32>(nSize)) + "\"";
            rWrt.Strm().WriteOString( sOut );

            if( rWrt.m_bCfgOutStyles && rWrt.m_bTextAttr )
            {
                // always export font size as CSS option, too
                OutCSS1_HintStyleOpt( rWrt, rHt );
            }
        }
        rWrt.Strm().WriteChar( '>' );
    }
    else
    {
        if (rWrt.mbXHTML)
            HTMLOutFuncs::Out_AsciiTag(
                rWrt.Strm(), Concat2View(rWrt.GetNamespace() + OOO_STRING_SVTOOLS_HTML_span), false);
        else
            HTMLOutFuncs::Out_AsciiTag( rWrt.Strm(), Concat2View(rWrt.GetNamespace() + OOO_STRING_SVTOOLS_HTML_font), false );
    }

    return rWrt;
}

static SwHTMLWriter& OutHTML_SvxLanguage( SwHTMLWriter& rWrt, const SfxPoolItem& rHt )
{
    if( rWrt.m_bOutOpts )
        return rWrt;

    LanguageType eLang = static_cast<const SvxLanguageItem &>(rHt).GetLanguage();
    if( LANGUAGE_DONTKNOW == eLang )
        return rWrt;

    if( rWrt.m_bTagOn )
    {
        OString sOut = "<" + rWrt.GetNamespace() + OOO_STRING_SVTOOLS_HTML_span;
        rWrt.Strm().WriteOString( sOut );
        rWrt.OutLanguage( static_cast<const SvxLanguageItem &>(rHt).GetLanguage() );
        rWrt.Strm().WriteChar( '>' );
    }
    else
    {
        HTMLOutFuncs::Out_AsciiTag( rWrt.Strm(), Concat2View(rWrt.GetNamespace() + OOO_STRING_SVTOOLS_HTML_span), false );
    }

    return rWrt;
}
static SwHTMLWriter& OutHTML_SwWeight( SwHTMLWriter& rWrt, const SfxPoolItem& rHt )
{
    if( rWrt.m_bOutOpts )
        return rWrt;

    const FontWeight nBold = static_cast<const SvxWeightItem&>(rHt).GetWeight();
    if( WEIGHT_BOLD == nBold )
    {
        HTMLOutFuncs::Out_AsciiTag( rWrt.Strm(), Concat2View(rWrt.GetNamespace() + OOO_STRING_SVTOOLS_HTML_bold), rWrt.m_bTagOn );
    }
    else if( rWrt.m_bCfgOutStyles && rWrt.m_bTextAttr )
    {
        // maybe as CSS1 attribute ?
        OutCSS1_HintSpanTag( rWrt, rHt );
    }

    return rWrt;
}

static SwHTMLWriter& OutHTML_SwCrossedOut( SwHTMLWriter& rWrt, const SfxPoolItem& rHt )
{
    if( rWrt.m_bOutOpts )
        return rWrt;

    // Because of Netscape, we output STRIKE and not S!
    const FontStrikeout nStrike = static_cast<const SvxCrossedOutItem&>(rHt).GetStrikeout();
    if( STRIKEOUT_NONE != nStrike && !rWrt.mbReqIF )
    {
        HTMLOutFuncs::Out_AsciiTag( rWrt.Strm(), Concat2View(rWrt.GetNamespace() + OOO_STRING_SVTOOLS_HTML_strike), rWrt.m_bTagOn );
    }
    else if( rWrt.m_bCfgOutStyles && rWrt.m_bTextAttr )
    {
        // maybe as CSS1 attribute?
        OutCSS1_HintSpanTag( rWrt, rHt );
    }

    return rWrt;
}

static SwHTMLWriter& OutHTML_SvxEscapement( SwHTMLWriter& rWrt, const SfxPoolItem& rHt )
{
    if( rWrt.m_bOutOpts )
        return rWrt;

    const SvxEscapement eEscape = static_cast<const SvxEscapementItem&>(rHt).GetEscapement();
    OString aTag;
    switch( eEscape )
    {
    case SvxEscapement::Superscript: aTag = OOO_STRING_SVTOOLS_HTML_superscript ""_ostr; break;
    case SvxEscapement::Subscript: aTag = OOO_STRING_SVTOOLS_HTML_subscript ""_ostr; break;
    default:
        ;
    }

    if( !aTag.isEmpty() )
    {
        HTMLOutFuncs::Out_AsciiTag( rWrt.Strm(), Concat2View(rWrt.GetNamespace() + aTag), rWrt.m_bTagOn );
    }
    else if( rWrt.m_bCfgOutStyles && rWrt.m_bTextAttr )
    {
        // maybe as CSS1 attribute?
        OutCSS1_HintSpanTag( rWrt, rHt );
    }

    return rWrt;
}

static SwHTMLWriter& OutHTML_SwUnderline( SwHTMLWriter& rWrt, const SfxPoolItem& rHt )
{
    if( rWrt.m_bOutOpts )
        return rWrt;

    const FontLineStyle eUnder = static_cast<const SvxUnderlineItem&>(rHt).GetLineStyle();
    if( LINESTYLE_NONE != eUnder && !rWrt.mbReqIF )
    {
        HTMLOutFuncs::Out_AsciiTag( rWrt.Strm(), Concat2View(rWrt.GetNamespace() + OOO_STRING_SVTOOLS_HTML_underline), rWrt.m_bTagOn );
    }
    else if( rWrt.m_bCfgOutStyles && rWrt.m_bTextAttr )
    {
        // maybe as CSS1 attribute?
        OutCSS1_HintSpanTag( rWrt, rHt );
    }

    return rWrt;
}

static SwHTMLWriter& OutHTML_SwFlyCnt( SwHTMLWriter& rWrt, const SfxPoolItem& rHt )
{
    const SwFormatFlyCnt& rFlyCnt = static_cast<const SwFormatFlyCnt&>(rHt);

    const SwFrameFormat& rFormat = *rFlyCnt.GetFrameFormat();
    const SdrObject *pSdrObj = nullptr;

    SwHTMLFrameType eType = rWrt.GuessFrameType( rFormat, pSdrObj );
    AllHtmlFlags nMode = getHTMLOutFrameAsCharTable(eType, rWrt.m_nExportMode);
    rWrt.OutFrameFormat( nMode, rFormat, pSdrObj );
    return rWrt;
}

// This is now our Blink item. Blinking is activated by setting the item to
// true!
static SwHTMLWriter& OutHTML_SwBlink( SwHTMLWriter& rWrt, const SfxPoolItem& rHt )
{
    if( rWrt.m_bOutOpts )
        return rWrt;

    if( static_cast<const SvxBlinkItem&>(rHt).GetValue() )
    {
        HTMLOutFuncs::Out_AsciiTag( rWrt.Strm(), Concat2View(rWrt.GetNamespace() + OOO_STRING_SVTOOLS_HTML_blink), rWrt.m_bTagOn );
    }
    else if( rWrt.m_bCfgOutStyles && rWrt.m_bTextAttr )
    {
        // maybe as CSS1 attribute?
        OutCSS1_HintSpanTag( rWrt, rHt );
    }

    return rWrt;
}

SwHTMLWriter& OutHTML_INetFormat( SwHTMLWriter& rWrt, const SwFormatINetFormat& rINetFormat, bool bOn )
{
    OUString aURL( rINetFormat.GetValue() );
    const SvxMacroTableDtor *pMacTable = rINetFormat.GetMacroTable();
    bool bEvents = pMacTable != nullptr && !pMacTable->empty();

    // Anything to output at all?
    if( aURL.isEmpty() && !bEvents && rINetFormat.GetName().isEmpty() )
        return rWrt;

    // bOn controls if we are writing the opening or closing tag
    if( !bOn )
    {
        HTMLOutFuncs::Out_AsciiTag( rWrt.Strm(), Concat2View(rWrt.GetNamespace() + OOO_STRING_SVTOOLS_HTML_anchor), false );
        return rWrt;
    }

    OString sOut("<" + rWrt.GetNamespace() + OOO_STRING_SVTOOLS_HTML_anchor);

    bool bScriptDependent = false;
    {
        const SwCharFormat* pFormat = rWrt.m_pDoc->getIDocumentStylePoolAccess().GetCharFormatFromPool(
                 RES_POOLCHR_INET_NORMAL );
        std::unique_ptr<SwHTMLFormatInfo> pFormatInfo(new SwHTMLFormatInfo(pFormat));
        auto const it = rWrt.m_CharFormatInfos.find( pFormatInfo );
        if (it != rWrt.m_CharFormatInfos.end())
        {
            bScriptDependent = (*it)->bScriptDependent;
        }
    }
    if( !bScriptDependent )
    {
        const SwCharFormat* pFormat = rWrt.m_pDoc->getIDocumentStylePoolAccess().GetCharFormatFromPool(
                 RES_POOLCHR_INET_VISIT );
        std::unique_ptr<SwHTMLFormatInfo> pFormatInfo(new SwHTMLFormatInfo(pFormat));
        auto const it = rWrt.m_CharFormatInfos.find( pFormatInfo );
        if (it != rWrt.m_CharFormatInfos.end())
        {
            bScriptDependent = (*it)->bScriptDependent;
        }
    }

    if( bScriptDependent )
    {
        sOut += " " OOO_STRING_SVTOOLS_HTML_O_class "=\"";
        const char* pStr = nullptr;
        switch( rWrt.m_nCSS1Script )
        {
        case CSS1_OUTMODE_WESTERN:
            pStr = "western";
            break;
        case CSS1_OUTMODE_CJK:
            pStr = "cjk";
            break;
        case CSS1_OUTMODE_CTL:
            pStr = "ctl";
            break;
        }
        sOut += pStr + OString::Concat("\"");
    }

    rWrt.Strm().WriteOString( sOut );
    sOut = ""_ostr;

    OUString sRel;

    if( !aURL.isEmpty() || bEvents )
    {
        OUString sTmp( aURL.toAsciiUpperCase() );
        sal_Int32 nPos = sTmp.indexOf( "\" REL=" );
        if( nPos >= 0 )
        {
            sRel = aURL.copy( nPos+1 );
            aURL = aURL.copy( 0, nPos);
        }
        aURL = comphelper::string::strip(aURL, ' ');

        sOut += " " OOO_STRING_SVTOOLS_HTML_O_href "=\"";
        rWrt.Strm().WriteOString( sOut );
        rWrt.OutHyperlinkHRefValue( aURL );
        sOut = "\""_ostr;
    }

    if( !rINetFormat.GetName().isEmpty() )
    {
        sOut += " " OOO_STRING_SVTOOLS_HTML_O_name "=\"";
        rWrt.Strm().WriteOString( sOut );
        HTMLOutFuncs::Out_String( rWrt.Strm(), rINetFormat.GetName() );
        sOut = "\""_ostr;
    }

    if (!rWrt.mbReqIF) // no target attribute for ReqIF
    {
        const OUString& rTarget = rINetFormat.GetTargetFrame();
        if (!rTarget.isEmpty())
        {
            sOut += " " OOO_STRING_SVTOOLS_HTML_O_target "=\"";
            rWrt.Strm().WriteOString(sOut);
            HTMLOutFuncs::Out_String(rWrt.Strm(), rTarget);
            sOut = "\""_ostr;
        }
    }

    if( !sRel.isEmpty() )
        sOut += OUStringToOString(sRel, RTL_TEXTENCODING_ASCII_US);

    if( !sOut.isEmpty() )
        rWrt.Strm().WriteOString( sOut );

    if( bEvents )
        HTMLOutFuncs::Out_Events( rWrt.Strm(), *pMacTable, aAnchorEventTable,
                                  rWrt.m_bCfgStarBasic );
    rWrt.Strm().WriteOString( ">" );

    return rWrt;
}

static SwHTMLWriter& OutHTML_SwFormatINetFormat( SwHTMLWriter& rWrt, const SfxPoolItem& rHt )
{
    if( rWrt.m_bOutOpts )
        return rWrt;

    const SwFormatINetFormat& rINetFormat = static_cast<const SwFormatINetFormat&>(rHt);

    if( rWrt.m_bTagOn )
    {
        // if necessary, temporarily close an attribute that is still open
        if( !rWrt.m_aINetFormats.empty() )
        {
            SwFormatINetFormat *pINetFormat =
                rWrt.m_aINetFormats.back();
            OutHTML_INetFormat( rWrt, *pINetFormat, false );
        }

        // now, open the new one
        OutHTML_INetFormat( rWrt, rINetFormat, true );

        // and remember it
        SwFormatINetFormat *pINetFormat = new SwFormatINetFormat( rINetFormat );
        rWrt.m_aINetFormats.push_back( pINetFormat );
    }
    else
    {
        OutHTML_INetFormat( rWrt, rINetFormat, false );

        OSL_ENSURE( rWrt.m_aINetFormats.size(), "there must be a URL attribute missing" );
        if( !rWrt.m_aINetFormats.empty() )
        {
            // get its own attribute from the stack
            SwFormatINetFormat *pINetFormat = rWrt.m_aINetFormats.back();
            rWrt.m_aINetFormats.pop_back();
            delete pINetFormat;
        }

        if( !rWrt.m_aINetFormats.empty() )
        {
            // there is still an attribute on the stack that must be reopened
            SwFormatINetFormat *pINetFormat = rWrt.m_aINetFormats.back();
            OutHTML_INetFormat( rWrt, *pINetFormat, true );
        }
    }

    return rWrt;
}

static SwHTMLWriter& OutHTML_SwTextCharFormat( SwHTMLWriter& rWrt, const SfxPoolItem& rHt )
{
    if( rWrt.m_bOutOpts )
        return rWrt;

    const SwFormatCharFormat& rChrFormat = static_cast<const SwFormatCharFormat&>(rHt);
    const SwCharFormat* pFormat = rChrFormat.GetCharFormat();

    if( !pFormat )
    {
        return rWrt;
    }

    std::unique_ptr<SwHTMLFormatInfo> pTmpInfo(new SwHTMLFormatInfo(pFormat));
    SwHTMLFormatInfos::const_iterator it = rWrt.m_CharFormatInfos.find(pTmpInfo);
    if (it == rWrt.m_CharFormatInfos.end())
        return rWrt;

    const SwHTMLFormatInfo *pFormatInfo = it->get();
    assert(pFormatInfo && "Why is there no information about the character style?");

    if( rWrt.m_bTagOn )
    {
        OString sOut = "<" + rWrt.GetNamespace();
        if( !pFormatInfo->aToken.isEmpty() )
            sOut += pFormatInfo->aToken;
        else
            sOut += OOO_STRING_SVTOOLS_HTML_span;

        if( rWrt.m_bCfgOutStyles &&
            (!pFormatInfo->aClass.isEmpty() || pFormatInfo->bScriptDependent) )
        {
            sOut += " " OOO_STRING_SVTOOLS_HTML_O_class "=\"";
            rWrt.Strm().WriteOString( sOut );
            OUString aClass( pFormatInfo->aClass );
            if( pFormatInfo->bScriptDependent )
            {
                if( !aClass.isEmpty() )
                   aClass += "-";
                switch( rWrt.m_nCSS1Script )
                {
                case CSS1_OUTMODE_WESTERN:
                    aClass += "western";
                    break;
                case CSS1_OUTMODE_CJK:
                    aClass += "cjk";
                    break;
                case CSS1_OUTMODE_CTL:
                    aClass += "ctl";
                    break;
                }
            }
            HTMLOutFuncs::Out_String( rWrt.Strm(), aClass );
            sOut = "\""_ostr;
        }
        sOut += ">";
        rWrt.Strm().WriteOString( sOut );
    }
    else
    {
        OString aTag = !pFormatInfo->aToken.isEmpty() ? pFormatInfo->aToken.getStr()
                                                      : OOO_STRING_SVTOOLS_HTML_span;
        HTMLOutFuncs::Out_AsciiTag(rWrt.Strm(), Concat2View(rWrt.GetNamespace() + aTag), false);
    }

    return rWrt;
}

static SwHTMLWriter& OutHTML_SvxAdjust( SwHTMLWriter& rWrt, const SfxPoolItem& rHt )
{
    if( !rWrt.m_bOutOpts || !rWrt.m_bTagOn )
        return  rWrt;

    const SvxAdjustItem& rAdjust = static_cast<const SvxAdjustItem&>(rHt);
    const char* pStr = nullptr;
    switch( rAdjust.GetAdjust() )
    {
    case SvxAdjust::Center: pStr = OOO_STRING_SVTOOLS_HTML_AL_center; break;
    case SvxAdjust::Left: pStr = OOO_STRING_SVTOOLS_HTML_AL_left; break;
    case SvxAdjust::Right: pStr = OOO_STRING_SVTOOLS_HTML_AL_right; break;
    case SvxAdjust::Block: pStr = OOO_STRING_SVTOOLS_HTML_AL_justify; break;
    default:
        ;
    }
    if( pStr )
    {
        OString sOut = OString::Concat(" " OOO_STRING_SVTOOLS_HTML_O_align "=\"") +
            pStr + "\"";
        rWrt.Strm().WriteOString( sOut );
    }

    return rWrt;
}

/*
 * here, define the table for the HTML function pointers to the output
 * functions.
 */

const SwAttrFnTab aHTMLAttrFnTab = {
/* RES_CHRATR_CASEMAP   */          OutHTML_CSS1Attr,
/* RES_CHRATR_CHARSETCOLOR  */      nullptr,
/* RES_CHRATR_COLOR */              OutHTML_SvxColor,
/* RES_CHRATR_CONTOUR   */          nullptr,
/* RES_CHRATR_CROSSEDOUT    */      OutHTML_SwCrossedOut,
/* RES_CHRATR_ESCAPEMENT    */      OutHTML_SvxEscapement,
/* RES_CHRATR_FONT  */              OutHTML_SvxFont,
/* RES_CHRATR_FONTSIZE  */          OutHTML_SvxFontHeight,
/* RES_CHRATR_KERNING   */          OutHTML_CSS1Attr,
/* RES_CHRATR_LANGUAGE  */          OutHTML_SvxLanguage,
/* RES_CHRATR_POSTURE   */          OutHTML_SwPosture,
/* RES_CHRATR_UNUSED1*/             nullptr,
/* RES_CHRATR_SHADOWED  */          nullptr,
/* RES_CHRATR_UNDERLINE */          OutHTML_SwUnderline,
/* RES_CHRATR_WEIGHT    */          OutHTML_SwWeight,
/* RES_CHRATR_WORDLINEMODE  */      nullptr,
/* RES_CHRATR_AUTOKERN  */          nullptr,
/* RES_CHRATR_BLINK */              OutHTML_SwBlink,
/* RES_CHRATR_NOHYPHEN  */          nullptr, // New: don't hyphenate
/* RES_CHRATR_UNUSED2 */            nullptr,
/* RES_CHRATR_BACKGROUND */         OutHTML_CSS1Attr, // New: character background
/* RES_CHRATR_CJK_FONT */           OutHTML_SvxFont,
/* RES_CHRATR_CJK_FONTSIZE */       OutHTML_SvxFontHeight,
/* RES_CHRATR_CJK_LANGUAGE */       OutHTML_SvxLanguage,
/* RES_CHRATR_CJK_POSTURE */        OutHTML_SwPosture,
/* RES_CHRATR_CJK_WEIGHT */         OutHTML_SwWeight,
/* RES_CHRATR_CTL_FONT */           OutHTML_SvxFont,
/* RES_CHRATR_CTL_FONTSIZE */       OutHTML_SvxFontHeight,
/* RES_CHRATR_CTL_LANGUAGE */       OutHTML_SvxLanguage,
/* RES_CHRATR_CTL_POSTURE */        OutHTML_SwPosture,
/* RES_CHRATR_CTL_WEIGHT */         OutHTML_SwWeight,
/* RES_CHRATR_ROTATE */             nullptr,
/* RES_CHRATR_EMPHASIS_MARK */      nullptr,
/* RES_CHRATR_TWO_LINES */          nullptr,
/* RES_CHRATR_SCALEW */             nullptr,
/* RES_CHRATR_RELIEF */             nullptr,
/* RES_CHRATR_HIDDEN */             OutHTML_CSS1Attr,
/* RES_CHRATR_OVERLINE */           OutHTML_CSS1Attr,
/* RES_CHRATR_RSID */               nullptr,
/* RES_CHRATR_BOX */                OutHTML_CSS1Attr,
/* RES_CHRATR_SHADOW */             nullptr,
/* RES_CHRATR_HIGHLIGHT */          nullptr,
/* RES_CHRATR_GRABBAG */            nullptr,
/* RES_CHRATR_BIDIRTL */            nullptr,
/* RES_CHRATR_UNUSED3 */            nullptr,
/* RES_CHRATR_SCRIPT_HINT */        nullptr,

/* RES_TXTATR_REFMARK */            nullptr,
/* RES_TXTATR_TOXMARK */            nullptr,
/* RES_TXTATR_META */               nullptr,
/* RES_TXTATR_METAFIELD */          nullptr,
/* RES_TXTATR_AUTOFMT */            nullptr,
/* RES_TXTATR_INETFMT */            OutHTML_SwFormatINetFormat,
/* RES_TXTATR_CHARFMT */            OutHTML_SwTextCharFormat,
/* RES_TXTATR_CJK_RUBY */           nullptr,
/* RES_TXTATR_UNKNOWN_CONTAINER */  nullptr,
/* RES_TXTATR_INPUTFIELD */         OutHTML_SwFormatField,
/* RES_TXTATR_CONTENTCONTROL */     nullptr,

/* RES_TXTATR_FIELD */              OutHTML_SwFormatField,
/* RES_TXTATR_FLYCNT */             OutHTML_SwFlyCnt,
/* RES_TXTATR_FTN */                OutHTML_SwFormatFootnote,
/* RES_TXTATR_ANNOTATION */         OutHTML_SwFormatField,
/* RES_TXTATR_LINEBREAK */          OutHTML_SwFormatLineBreak,
/* RES_TXTATR_DUMMY1 */             nullptr, // Dummy:

/* RES_PARATR_LINESPACING   */      nullptr,
/* RES_PARATR_ADJUST    */          OutHTML_SvxAdjust,
/* RES_PARATR_SPLIT */              nullptr,
/* RES_PARATR_ORPHANS   */          nullptr,
/* RES_PARATR_WIDOWS    */          nullptr,
/* RES_PARATR_TABSTOP   */          nullptr,
/* RES_PARATR_HYPHENZONE*/          nullptr,
/* RES_PARATR_DROP */               OutHTML_CSS1Attr,
/* RES_PARATR_REGISTER */           nullptr, // new:  register-true
/* RES_PARATR_NUMRULE */            nullptr, // Dummy:
/* RES_PARATR_SCRIPTSPACE */        nullptr, // Dummy:
/* RES_PARATR_HANGINGPUNCTUATION */ nullptr, // Dummy:
/* RES_PARATR_FORBIDDEN_RULES */    nullptr, // new
/* RES_PARATR_VERTALIGN */          nullptr, // new
/* RES_PARATR_SNAPTOGRID*/          nullptr, // new
/* RES_PARATR_CONNECT_TO_BORDER */  nullptr, // new
/* RES_PARATR_OUTLINELEVEL */       nullptr,
/* RES_PARATR_RSID */               nullptr,
/* RES_PARATR_GRABBAG */            nullptr,

/* RES_PARATR_LIST_ID */            nullptr, // new
/* RES_PARATR_LIST_LEVEL */         nullptr, // new
/* RES_PARATR_LIST_ISRESTART */     nullptr, // new
/* RES_PARATR_LIST_RESTARTVALUE */  nullptr, // new
/* RES_PARATR_LIST_ISCOUNTED */     nullptr, // new

/* RES_FILL_ORDER   */              nullptr,
/* RES_FRM_SIZE */                  nullptr,
/* RES_PAPER_BIN    */              nullptr,
/* RES_MARGIN_FIRSTLINE */          nullptr,
/* RES_MARGIN_TEXTLEFT */           nullptr,
/* RES_MARGIN_RIGHT */              nullptr,
/* RES_MARGIN_LEFT */               nullptr,
/* RES_MARGIN_GUTTER */             nullptr,
/* RES_MARGIN_GUTTER_RIGHT */       nullptr,
/* RES_LR_SPACE */                  nullptr,
/* RES_UL_SPACE */                  nullptr,
/* RES_PAGEDESC */                  nullptr,
/* RES_BREAK */                     nullptr,
/* RES_CNTNT */                     nullptr,
/* RES_HEADER */                    nullptr,
/* RES_FOOTER */                    nullptr,
/* RES_PRINT */                     nullptr,
/* RES_OPAQUE */                    nullptr,
/* RES_PROTECT */                   nullptr,
/* RES_SURROUND */                  nullptr,
/* RES_VERT_ORIENT */               nullptr,
/* RES_HORI_ORIENT */               nullptr,
/* RES_ANCHOR */                    nullptr,
/* RES_BACKGROUND */                nullptr,
/* RES_BOX  */                      nullptr,
/* RES_SHADOW */                    nullptr,
/* RES_FRMMACRO */                  nullptr,
/* RES_COL */                       nullptr,
/* RES_KEEP */                      nullptr,
/* RES_URL */                       nullptr,
/* RES_EDIT_IN_READONLY */          nullptr,
/* RES_LAYOUT_SPLIT */              nullptr,
/* RES_CHAIN */                     nullptr,
/* RES_TEXTGRID */                  nullptr,
/* RES_LINENUMBER */                nullptr,
/* RES_FTN_AT_TXTEND */             nullptr,
/* RES_END_AT_TXTEND */             nullptr,
/* RES_COLUMNBALANCE */             nullptr,
/* RES_FRAMEDIR */                  nullptr,
/* RES_HEADER_FOOTER_EAT_SPACING */ nullptr,
/* RES_ROW_SPLIT */                 nullptr,
/* RES_FLY_SPLIT */                 nullptr,
/* RES_FOLLOW_TEXT_FLOW */          nullptr,
/* RES_COLLAPSING_BORDERS */        nullptr,
/* RES_WRAP_INFLUENCE_ON_OBJPOS */  nullptr,
/* RES_AUTO_STYLE */                nullptr,
/* RES_FRMATR_STYLE_NAME */         nullptr,
/* RES_FRMATR_CONDITIONAL_STYLE_NAME */ nullptr,
/* RES_FRMATR_GRABBAG */            nullptr,
/* RES_TEXT_VERT_ADJUST */          nullptr,
/* RES_BACKGROUND_FULL_SIZE */      nullptr,
/* RES_RTL_GUTTER */                nullptr,
/* RES_DECORATIVE */                nullptr,

/* RES_GRFATR_MIRRORGRF */          nullptr,
/* RES_GRFATR_CROPGRF   */          nullptr,
/* RES_GRFATR_ROTATION */           nullptr,
/* RES_GRFATR_LUMINANCE */          nullptr,
/* RES_GRFATR_CONTRAST */           nullptr,
/* RES_GRFATR_CHANNELR */           nullptr,
/* RES_GRFATR_CHANNELG */           nullptr,
/* RES_GRFATR_CHANNELB */           nullptr,
/* RES_GRFATR_GAMMA */              nullptr,
/* RES_GRFATR_INVERT */             nullptr,
/* RES_GRFATR_TRANSPARENCY */       nullptr,
/* RES_GRFATR_DRWAMODE */           nullptr,
/* RES_GRFATR_DUMMY3 */             nullptr,
/* RES_GRFATR_DUMMY4 */             nullptr,
/* RES_GRFATR_DUMMY5 */             nullptr,

/* RES_BOXATR_FORMAT */             nullptr,
/* RES_BOXATR_FORMULA */            nullptr,
/* RES_BOXATR_VALUE */              nullptr
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
