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

#include <sal/config.h>
#include <sal/log.hxx>
#include <com/sun/star/text/XTextSection.hpp>
#include <comphelper/propertyvalue.hxx>
#include <cppuhelper/typeprovider.hxx>
#include <vcl/svapp.hxx>

#include <pagedesc.hxx>
#include <poolfmt.hxx>
#include <redline.hxx>
#include <section.hxx>
#include <unoprnms.hxx>
#include <unosection.hxx>
#include <unotextrange.hxx>
#include <unotextcursor.hxx>
#include <unoparagraph.hxx>
#include <unocoll.hxx>
#include <unomap.hxx>
#include <unocrsr.hxx>
#include <unoport.hxx>
#include <unoredline.hxx>
#include <doc.hxx>
#include <IDocumentRedlineAccess.hxx>
#include <IDocumentStylePoolAccess.hxx>
#include <docary.hxx>
#include <unotbl.hxx>

#include <optional>

using namespace ::com::sun::star;

namespace
{
uno::Sequence<beans::PropertyValue> GetSuccessorProperties(const SwRangeRedline& rRedline)
{
    const SwRedlineData* pNext = rRedline.GetRedlineData().Next();
    if (pNext)
    {
        uno::Reference<text::XText> xRedlineText;
        if (pNext->GetType() == RedlineType::Delete)
        {
            // Something on delete: produce the XText for the underlying delete.
            const SwNodeIndex* pNodeIdx = rRedline.GetContentIdx();
            if (pNodeIdx
                && (pNodeIdx->GetNode().EndOfSectionIndex() - pNodeIdx->GetNode().GetIndex())
                       > SwNodeOffset(1))
            {
                // We have at least one node between the start and end node, create the
                // SwXRedlineText.
                SwDoc& rDoc = rRedline.GetDoc();
                xRedlineText = new SwXRedlineText(&rDoc, *pNodeIdx);
            }
        }

        return {
            // GetAuthorString(n) walks the SwRedlineData* chain;
            // here we always need element 1
            comphelper::makePropertyValue(UNO_NAME_REDLINE_AUTHOR, rRedline.GetAuthorString(1)),
            comphelper::makePropertyValue(UNO_NAME_REDLINE_DATE_TIME,
                                          pNext->GetTimeStamp().GetUNODateTime()),
            comphelper::makePropertyValue(UNO_NAME_REDLINE_COMMENT, pNext->GetComment()),
            comphelper::makePropertyValue(UNO_NAME_REDLINE_TYPE,
                                          SwRedlineTypeToOUString(pNext->GetType())),
            comphelper::makePropertyValue(UNO_NAME_REDLINE_TEXT, xRedlineText)
        };
    }
    return uno::Sequence<beans::PropertyValue>(5);
}

std::optional<uno::Any> GetRedlinePortionPropertyValue(std::u16string_view rPropertyName,
                                                       const SwRangeRedline& rRedline)
{
    uno::Any aRet;
    if (rPropertyName == UNO_NAME_REDLINE_AUTHOR)
    {
        aRet <<= rRedline.GetAuthorString();
    }
    else if (rPropertyName == UNO_NAME_REDLINE_DATE_TIME)
    {
        aRet <<= rRedline.GetTimeStamp().GetUNODateTime();
    }
    else if (rPropertyName == UNO_NAME_REDLINE_MOVED_ID)
    {
        aRet <<= rRedline.GetMovedID();
    }
    else if (rPropertyName == UNO_NAME_REDLINE_COMMENT)
    {
        aRet <<= rRedline.GetComment();
    }
    else if (rPropertyName == UNO_NAME_REDLINE_DESCRIPTION)
    {
        aRet <<= rRedline.GetDescr();
    }
    else if (rPropertyName == UNO_NAME_REDLINE_TYPE)
    {
        aRet <<= SwRedlineTypeToOUString(rRedline.GetType());
    }
    else if (rPropertyName == UNO_NAME_REDLINE_SUCCESSOR_DATA)
    {
        if (rRedline.GetRedlineData().Next())
            aRet <<= GetSuccessorProperties(rRedline);
    }
    else if (rPropertyName == UNO_NAME_REDLINE_IDENTIFIER)
    {
        aRet <<= OUString::number(reinterpret_cast<sal_IntPtr>(&rRedline));
    }
    else if (rPropertyName == UNO_NAME_IS_IN_HEADER_FOOTER)
    {
        aRet <<= rRedline.GetDoc().IsInHeaderFooter(rRedline.GetPoint()->GetNode());
    }
    else if (rPropertyName == UNO_NAME_MERGE_LAST_PARA)
    {
        aRet <<= !rRedline.IsDelLastPara();
    }
    else
    {
        return {}; // Property name unknown; the caller decides when to throw
    }
    return aRet;
}
}

SwXRedlineText::SwXRedlineText(SwDoc* _pDoc, const SwNodeIndex& aIndex) :
    SwXText(_pDoc, CursorType::Redline),
    m_aNodeIndex(aIndex)
{
}

const SwStartNode* SwXRedlineText::GetStartNode() const
{
    return m_aNodeIndex.GetNode().GetStartNode();
}

uno::Any SwXRedlineText::queryInterface( const uno::Type& rType )
{
    uno::Any aRet;

    if (cppu::UnoType<container::XEnumerationAccess>::get()== rType)
    {
        uno::Reference<container::XEnumerationAccess> aAccess = this;
        aRet <<= aAccess;
    }
    else
    {
        // delegate to SwXText and OWeakObject
        aRet = SwXText::queryInterface(rType);
        if(!aRet.hasValue())
        {
            aRet = OWeakObject::queryInterface(rType);
        }
    }

    return aRet;
}

uno::Sequence<uno::Type> SwXRedlineText::getTypes()
{
    return cppu::OTypeCollection(
            cppu::UnoType<container::XEnumerationAccess>::get(),
            SwXText::getTypes()
        ).getTypes();
}

uno::Sequence<sal_Int8> SwXRedlineText::getImplementationId()
{
    return css::uno::Sequence<sal_Int8>();
}

rtl::Reference< SwXTextCursor > SwXRedlineText::createXTextCursor()
{
    SolarMutexGuard aGuard;

    SwPosition aPos(m_aNodeIndex);
    rtl::Reference<SwXTextCursor> pXCursor =
        new SwXTextCursor(*GetDoc(), this, CursorType::Redline, aPos);
    auto& rUnoCursor(pXCursor->GetCursor());
    rUnoCursor.Move(fnMoveForward, GoInNode);

    // #101929# prevent a newly created text cursor from running inside a table
    // because table cells have their own XText.
    // Patterned after SwXTextFrame::createTextCursor().

    // skip all tables at the beginning
    SwTableNode* pTableNode = rUnoCursor.GetPointNode().FindTableNode();
    bool bTable = pTableNode != nullptr;
    while( pTableNode != nullptr )
    {
        rUnoCursor.GetPoint()->Assign( *pTableNode->EndOfSectionNode() );
        SwContentNode* pContentNode = SwNodes::GoNext(rUnoCursor.GetPoint());
        pTableNode = pContentNode->FindTableNode();
    }
    if( bTable && rUnoCursor.GetPointNode().FindStartNodeByType( SwNormalStartNode )
                                                            != GetStartNode() )
    {
        // We have gone too far and have left our own redline. This means that
        // no content node outside of a table could be found, and therefore we
        // except.
        throw uno::RuntimeException(
            u"No content node found that is inside this change section "
            "but outside of a table"_ustr);
    }

    return pXCursor;
}

rtl::Reference< SwXTextCursor > SwXRedlineText::createXTextCursorByRange(
    const uno::Reference<text::XTextRange> & aTextRange)
{
    rtl::Reference< SwXTextCursor > xCursor = createXTextCursor();
    xCursor->gotoRange(aTextRange->getStart(), false);
    xCursor->gotoRange(aTextRange->getEnd(), true);
    return xCursor;
}

uno::Reference<container::XEnumeration> SwXRedlineText::createEnumeration()
{
    SolarMutexGuard aGuard;
    SwPaM aPam(m_aNodeIndex);
    aPam.Move(fnMoveForward, GoInNode);
    auto pUnoCursor(GetDoc()->CreateUnoCursor(*aPam.Start()));
    return SwXParagraphEnumeration::Create(this, pUnoCursor, CursorType::Redline);
}

uno::Type SwXRedlineText::getElementType(  )
{
    return cppu::UnoType<text::XTextRange>::get();
}

sal_Bool SwXRedlineText::hasElements(  )
{
    return true;    // we always have a content index
}

SwXRedlinePortion::SwXRedlinePortion(SwRangeRedline const& rRedline,
        SwUnoCursor const*const pPortionCursor,
        uno::Reference< SwXText > const& xParent, bool const bStart)
    : SwXTextPortion(pPortionCursor, xParent,
            bStart ? PORTION_REDLINE_START : PORTION_REDLINE_END)
    , m_rRedline(rRedline)
{
    SetCollapsed(!m_rRedline.HasMark());
}

SwXRedlinePortion::~SwXRedlinePortion()
{
}

uno::Any SwXRedlinePortion::getPropertyValue( const OUString& rPropertyName )
{
    SolarMutexGuard aGuard;
    if (!Validate())
    {
        return uno::Any();
    }
    uno::Any aRet;
    if(rPropertyName == UNO_NAME_REDLINE_TEXT)
    {
        const SwNodeIndex* pNodeIdx = m_rRedline.GetContentIdx();
        if(pNodeIdx )
        {
            if ( SwNodeOffset(1) < ( pNodeIdx->GetNode().EndOfSectionIndex() - pNodeIdx->GetNode().GetIndex() ) )
            {
                SwUnoCursor& rUnoCursor = GetCursor();
                uno::Reference<text::XText> xRet = new SwXRedlineText(&rUnoCursor.GetDoc(), *pNodeIdx);
                aRet <<= xRet;
            }
            else {
                OSL_FAIL("Empty section in redline portion! (end node immediately follows start node)");
            }
        }
    }
    else
    {
        if (auto oVal = GetRedlinePortionPropertyValue(rPropertyName, m_rRedline))
        {
            aRet = *oVal;
        }
        else
        {
            aRet = SwXTextPortion::getPropertyValue(rPropertyName);
        }
    }
    return aRet;
}

bool SwXRedlinePortion::Validate()
{
    SwUnoCursor& rUnoCursor = GetCursor();
    //search for the redline
    SwDoc& rDoc = rUnoCursor.GetDoc();
    const SwRedlineTable& rRedTable = rDoc.getIDocumentRedlineAccess().GetRedlineTable();
    bool bFound = false;
    for(size_t nRed = 0; nRed < rRedTable.size() && !bFound; nRed++)
    {
        bFound = &m_rRedline == rRedTable[nRed];
    }
    return bFound;
    // don't throw; the only caller can return void instead
}

uno::Sequence< sal_Int8 > SAL_CALL SwXRedlinePortion::getImplementationId(  )
{
    return css::uno::Sequence<sal_Int8>();
}

uno::Sequence< beans::PropertyValue > SwXRedlinePortion::CreateRedlineProperties(
    const SwRangeRedline& rRedline, bool bIsStart )
{
    uno::Sequence< beans::PropertyValue > aRet(12);
    const SwRedlineData* pNext = rRedline.GetRedlineData().Next();
    beans::PropertyValue* pRet = aRet.getArray();

    sal_Int32 nPropIdx  = 0;
    pRet[nPropIdx].Name = UNO_NAME_REDLINE_AUTHOR;
    pRet[nPropIdx++].Value <<= rRedline.GetAuthorString();
    pRet[nPropIdx].Name = UNO_NAME_REDLINE_DATE_TIME;
    pRet[nPropIdx++].Value <<= rRedline.GetTimeStamp().GetUNODateTime();
    pRet[nPropIdx].Name = UNO_NAME_REDLINE_COMMENT;
    pRet[nPropIdx++].Value <<= rRedline.GetComment();
    pRet[nPropIdx].Name = UNO_NAME_REDLINE_DESCRIPTION;
    pRet[nPropIdx++].Value <<= rRedline.GetDescr();
    pRet[nPropIdx].Name = UNO_NAME_REDLINE_TYPE;
    pRet[nPropIdx++].Value <<= SwRedlineTypeToOUString(rRedline.GetType());
    pRet[nPropIdx].Name = UNO_NAME_REDLINE_IDENTIFIER;
    pRet[nPropIdx++].Value <<= OUString::number(
        sal::static_int_cast< sal_Int64 >( reinterpret_cast< sal_IntPtr >(&rRedline) ) );
    pRet[nPropIdx].Name = UNO_NAME_IS_COLLAPSED;
    pRet[nPropIdx++].Value <<= !rRedline.HasMark();

    pRet[nPropIdx].Name = UNO_NAME_IS_START;
    pRet[nPropIdx++].Value <<= bIsStart;

    pRet[nPropIdx].Name = UNO_NAME_MERGE_LAST_PARA;
    pRet[nPropIdx++].Value <<= !rRedline.IsDelLastPara();

    const SwNodeIndex* pNodeIdx = rRedline.GetContentIdx();
    if(pNodeIdx )
    {
        if ( SwNodeOffset(1) < ( pNodeIdx->GetNode().EndOfSectionIndex() - pNodeIdx->GetNode().GetIndex() ) )
        {
            uno::Reference<text::XText> xRet = new SwXRedlineText(&rRedline.GetDoc(), *pNodeIdx);
            pRet[nPropIdx].Name = UNO_NAME_REDLINE_TEXT;
            pRet[nPropIdx++].Value <<= xRet;
        }
        else {
            OSL_FAIL("Empty section in redline portion! (end node immediately follows start node)");
        }
    }
    if(pNext)
    {
        pRet[nPropIdx].Name = UNO_NAME_REDLINE_SUCCESSOR_DATA;
        pRet[nPropIdx++].Value <<= GetSuccessorProperties(rRedline);
    }
    aRet.realloc(nPropIdx);
    return aRet;
}

SwXRedline::SwXRedline(SwRangeRedline& rRedline) :
    SwXText(&rRedline.GetDoc(), CursorType::Redline),
    m_pRedline(&rRedline)
{
    StartListening(GetDoc()->getIDocumentStylePoolAccess().GetPageDescFromPool(RES_POOLPAGE_STANDARD)->GetNotifier());
}

SwXRedline::~SwXRedline()
{
}

uno::Reference< beans::XPropertySetInfo > SwXRedline::getPropertySetInfo(  )
{
    static uno::Reference< beans::XPropertySetInfo >  xRef =
        aSwMapProvider.GetPropertySet(PROPERTY_MAP_REDLINE)->getPropertySetInfo();
    return xRef;
}

void SwXRedline::setPropertyValue( const OUString& rPropertyName, const uno::Any& aValue )
{
    SolarMutexGuard aGuard;
    if (!GetDoc())
        throw uno::RuntimeException();
    if(rPropertyName == UNO_NAME_REDLINE_AUTHOR)
    {
        OSL_FAIL("currently not available");
    }
    else if(rPropertyName == UNO_NAME_REDLINE_DATE_TIME)
    {
        OSL_FAIL("currently not available");
    }
    else if(rPropertyName == UNO_NAME_REDLINE_COMMENT)
    {
        OUString sTmp; aValue >>= sTmp;
        m_pRedline->SetComment(sTmp);
    }
    else if(rPropertyName == UNO_NAME_REDLINE_DESCRIPTION)
    {
        SAL_WARN("sw.uno", "SwXRedline::setPropertyValue: can't set Description");
    }
    else if(rPropertyName == UNO_NAME_REDLINE_TYPE)
    {
        OSL_FAIL("currently not available");
        OUString sTmp; aValue >>= sTmp;
        if(sTmp.isEmpty())
            throw lang::IllegalArgumentException();
    }
    else if(rPropertyName == UNO_NAME_REDLINE_SUCCESSOR_DATA)
    {
        OSL_FAIL("currently not available");
    }
    else
    {
        throw lang::IllegalArgumentException();
    }
}

uno::Any SwXRedline::getPropertyValue( const OUString& rPropertyName )
{
    SolarMutexGuard aGuard;
    if (!GetDoc())
        throw uno::RuntimeException();
    uno::Any aRet;
    bool bStart = rPropertyName == UNO_NAME_REDLINE_START;
    if(bStart ||
        rPropertyName == UNO_NAME_REDLINE_END)
    {
        uno::Reference<XInterface> xRet;
        SwPosition* pPoint = bStart ? m_pRedline->Start() : m_pRedline->End();
        switch (pPoint->GetNode().GetNodeType())
        {
            case SwNodeType::Section:
            {
                SwSectionNode* pSectNode = pPoint->GetNode().GetSectionNode();
                assert(pSectNode);
                SwSectionFormat* pSectionFormat = pSectNode->GetSection().GetFormat();
                xRet = cppu::getXWeak(SwXTextSection::CreateXTextSection(pSectionFormat).get());
            }
            break;
            case SwNodeType::Table :
            {
                SwTableNode* pTableNode = pPoint->GetNode().GetTableNode();
                assert(pTableNode);
                SwFrameFormat* pTableFormat = pTableNode->GetTable().GetFrameFormat();
                xRet = cppu::getXWeak(SwXTextTable::CreateXTextTable(pTableFormat).get());
            }
            break;
            case SwNodeType::Text :
            {
                xRet = cppu::getXWeak(SwXTextRange::CreateXTextRange(*GetDoc(), *pPoint, nullptr).get());
            }
            break;
            default:
                OSL_FAIL("illegal node type");
        }
        aRet <<= xRet;
    }
    else if(rPropertyName == UNO_NAME_REDLINE_TEXT)
    {
        const SwNodeIndex* pNodeIdx = m_pRedline->GetContentIdx();
        if( pNodeIdx )
        {
            if ( SwNodeOffset(1) < ( pNodeIdx->GetNode().EndOfSectionIndex() - pNodeIdx->GetNode().GetIndex() ) )
            {
                uno::Reference<text::XText> xRet = new SwXRedlineText(GetDoc(), *pNodeIdx);
                aRet <<= xRet;
            }
            else {
                OSL_FAIL("Empty section in redline portion! (end node immediately follows start node)");
            }
        }
    }
    else
    {
        if (auto oVal = GetRedlinePortionPropertyValue(rPropertyName, *m_pRedline))
            aRet = *oVal;
        else
            throw beans::UnknownPropertyException("Unknown property: " + rPropertyName, getXWeak());
    }
    return aRet;
}

void SwXRedline::addPropertyChangeListener(
    const OUString& /*aPropertyName*/,
    const uno::Reference< beans::XPropertyChangeListener >& /*xListener*/ )
{
}

void SwXRedline::removePropertyChangeListener(
    const OUString& /*aPropertyName*/, const uno::Reference< beans::XPropertyChangeListener >& /*aListener*/ )
{
}

void SwXRedline::addVetoableChangeListener(
    const OUString& /*PropertyName*/, const uno::Reference< beans::XVetoableChangeListener >& /*aListener*/ )
{
}

void SwXRedline::removeVetoableChangeListener(
    const OUString& /*PropertyName*/, const uno::Reference< beans::XVetoableChangeListener >& /*aListener*/ )
{
}

void SwXRedline::Notify( const SfxHint& rHint )
{
    if(rHint.GetId() == SfxHintId::Dying)
    {
        SetDoc(nullptr);
        m_pRedline = nullptr;
    }
    else if(rHint.GetId() == SfxHintId::SwFindRedline)
    {
        auto pHint = static_cast<const sw::FindRedlineHint*>(&rHint);
        if(!*pHint->m_ppXRedline && &pHint->m_rRedline == GetRedline())
            *pHint->m_ppXRedline = this;
    }
}

uno::Reference< container::XEnumeration >  SwXRedline::createEnumeration()
{
    SolarMutexGuard aGuard;
    if (!GetDoc())
        throw uno::RuntimeException();

    const SwNodeIndex* pNodeIndex = m_pRedline->GetContentIdx();
    if(!pNodeIndex)
        return nullptr;
    SwPaM aPam(*pNodeIndex);
    aPam.Move(fnMoveForward, GoInNode);
    auto pUnoCursor(GetDoc()->CreateUnoCursor(*aPam.Start()));
    return SwXParagraphEnumeration::Create(this, pUnoCursor, CursorType::Redline);
}

uno::Type SwXRedline::getElementType(  )
{
    return cppu::UnoType<text::XTextRange>::get();
}

sal_Bool SwXRedline::hasElements(  )
{
    if (!GetDoc())
        throw uno::RuntimeException();
    return nullptr != m_pRedline->GetContentIdx();
}

rtl::Reference< SwXTextCursor >  SwXRedline::createXTextCursor()
{
    if (!GetDoc())
        throw uno::RuntimeException();

    const SwNodeIndex* pNodeIndex = m_pRedline->GetContentIdx();
    if(!pNodeIndex)
    {
        throw uno::RuntimeException();
    }

    SwPosition aPos(*pNodeIndex);
    rtl::Reference<SwXTextCursor> pXCursor =
        new SwXTextCursor(*GetDoc(), this, CursorType::Redline, aPos);
    auto& rUnoCursor(pXCursor->GetCursor());
    rUnoCursor.Move(fnMoveForward, GoInNode);

    // is here a table?
    SwTableNode* pTableNode = rUnoCursor.GetPointNode().FindTableNode();
    while( pTableNode )
    {
        rUnoCursor.GetPoint()->Assign( *pTableNode->EndOfSectionNode() );
        SwContentNode* pCont = SwNodes::GoNext(rUnoCursor.GetPoint());
        pTableNode = pCont->FindTableNode();
    }

    return pXCursor;
}

rtl::Reference< SwXTextCursor > SwXRedline::createXTextCursorByRange(
    const uno::Reference< text::XTextRange > & /*aTextPosition*/)
{
    throw uno::RuntimeException();
}

uno::Any SwXRedline::queryInterface( const uno::Type& rType )
{
    uno::Any aRet = SwXText::queryInterface(rType);
    if(!aRet.hasValue())
    {
        aRet = SwXRedlineBaseClass::queryInterface(rType);
    }
    return aRet;
}

uno::Sequence<uno::Type> SwXRedline::getTypes()
{
    return comphelper::concatSequences(
            SwXText::getTypes(),
            SwXRedlineBaseClass::getTypes()
        );
}

uno::Sequence<sal_Int8> SwXRedline::getImplementationId()
{
    return css::uno::Sequence<sal_Int8>();
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
