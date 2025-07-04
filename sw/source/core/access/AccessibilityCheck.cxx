/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#include <AccessibilityCheck.hxx>
#include <AccessibilityIssue.hxx>
#include <AccessibilityCheckStrings.hrc>
#include <strings.hrc>
#include <ndnotxt.hxx>
#include <ndtxt.hxx>
#include <docsh.hxx>
#include <wrtsh.hxx>
#include <IDocumentDrawModelAccess.hxx>
#include <drawdoc.hxx>
#include <svx/svdpage.hxx>
#include <sortedobjs.hxx>
#include <swtable.hxx>
#include <com/sun/star/awt/FontWeight.hpp>
#include <com/sun/star/frame/XModel.hpp>
#include <com/sun/star/text/XTextContent.hpp>
#include <com/sun/star/document/XDocumentPropertiesSupplier.hpp>
#include <com/sun/star/style/XStyleFamiliesSupplier.hpp>
#include <officecfg/Office/Common.hxx>
#include <unoparagraph.hxx>
#include <unotools/intlwrapper.hxx>
#include <tools/urlobj.hxx>
#include <editeng/langitem.hxx>
#include <editeng/ulspitem.hxx>
#include <calbck.hxx>
#include <charatr.hxx>
#include <svx/xfillit0.hxx>
#include <svx/xflclit.hxx>
#include <ftnidx.hxx>
#include <authfld.hxx>
#include <txtftn.hxx>
#include <txtfrm.hxx>
#include <svl/itemiter.hxx>
#include <o3tl/string_view.hxx>
#include <o3tl/vector_utils.hxx>
#include <svx/swframetypes.hxx>
#include <fmtanchr.hxx>
#include <fmturl.hxx>
#include <dcontact.hxx>
#include <unotext.hxx>
#include <svx/svdoashp.hxx>
#include <svx/sdasitm.hxx>
#include <ndgrf.hxx>
#include <svl/fstathelper.hxx>
#include <osl/file.h>
#include <unotxdoc.hxx>

namespace sw
{
namespace
{
SwTextNode* lclSearchNextTextNode(SwNode* pCurrent)
{
    SwTextNode* pTextNode = nullptr;

    auto nIndex = pCurrent->GetIndex();
    auto nCount = pCurrent->GetNodes().Count();

    nIndex++; // go to next node

    while (pTextNode == nullptr && nIndex < nCount)
    {
        auto pNode = pCurrent->GetNodes()[nIndex];
        if (pNode->IsTextNode())
            pTextNode = pNode->GetTextNode();
        nIndex++;
    }

    return pTextNode;
}

void lcl_SetHiddenIssues(const std::shared_ptr<sw::AccessibilityIssue>& pIssue)
{
    switch (pIssue->m_eIssueID)
    {
        case sfx::AccessibilityIssueID::DOCUMENT_TITLE:
        {
            if (!officecfg::Office::Common::AccessibilityIssues::DocumentTitle::get())
                pIssue->setHidden(true);
        }
        break;
        case sfx::AccessibilityIssueID::DOCUMENT_LANGUAGE:
        {
            if (!officecfg::Office::Common::AccessibilityIssues::DocumentLanguage::get())
                pIssue->setHidden(true);
        }
        break;
        case sfx::AccessibilityIssueID::DOCUMENT_BACKGROUND:
        {
            if (!officecfg::Office::Common::AccessibilityIssues::DocumentBackground::get())
                pIssue->setHidden(true);
        }
        break;
        case sfx::AccessibilityIssueID::STYLE_LANGUAGE:
        {
            if (!officecfg::Office::Common::AccessibilityIssues::DocumentStyleLanguage::get())
                pIssue->setHidden(true);
        }
        break;
        case sfx::AccessibilityIssueID::LINKED_GRAPHIC:
        {
            if (!officecfg::Office::Common::AccessibilityIssues::LinkedGraphic::get())
                pIssue->setHidden(true);
        }
        break;
        case sfx::AccessibilityIssueID::NO_ALT_OLE:
        {
            if (!officecfg::Office::Common::AccessibilityIssues::NoAltOleObj::get())
                pIssue->setHidden(true);
        }
        break;
        case sfx::AccessibilityIssueID::NO_ALT_GRAPHIC:
        {
            if (!officecfg::Office::Common::AccessibilityIssues::NoAltGraphicObj::get())
                pIssue->setHidden(true);
        }
        break;
        case sfx::AccessibilityIssueID::NO_ALT_SHAPE:
        {
            if (!officecfg::Office::Common::AccessibilityIssues::NoAltShapeObj::get())
                pIssue->setHidden(true);
        }
        break;
        case sfx::AccessibilityIssueID::TABLE_MERGE_SPLIT:
        {
            if (!officecfg::Office::Common::AccessibilityIssues::TableMergeSplit::get())
                pIssue->setHidden(true);
        }
        break;
        case sfx::AccessibilityIssueID::TEXT_NEW_LINES:
        {
            if (!officecfg::Office::Common::AccessibilityIssues::TextNewLines::get())
                pIssue->setHidden(true);
        }
        break;
        case sfx::AccessibilityIssueID::TEXT_SPACES:
        {
            if (!officecfg::Office::Common::AccessibilityIssues::TextSpaces::get())
                pIssue->setHidden(true);
        }
        break;
        case sfx::AccessibilityIssueID::TEXT_TABS:
        {
            if (!officecfg::Office::Common::AccessibilityIssues::TextTabs::get())
                pIssue->setHidden(true);
        }
        break;
        case sfx::AccessibilityIssueID::TEXT_EMPTY_NUM_PARA:
        {
            if (!officecfg::Office::Common::AccessibilityIssues::TextEmptyNums::get())
                pIssue->setHidden(true);
        }
        break;
        case sfx::AccessibilityIssueID::TABLE_FORMATTING:
        {
            if (!officecfg::Office::Common::AccessibilityIssues::TableFormattings::get())
                pIssue->setHidden(true);
        }
        break;
        case sfx::AccessibilityIssueID::DIRECT_FORMATTING:
        {
            if (!officecfg::Office::Common::AccessibilityIssues::DirectFormattings::get())
                pIssue->setHidden(true);
        }
        break;
        case sfx::AccessibilityIssueID::HYPERLINK_IS_TEXT:
        {
            if (!officecfg::Office::Common::AccessibilityIssues::HyperlinkText::get())
                pIssue->setHidden(true);
        }
        break;
        case sfx::AccessibilityIssueID::HYPERLINK_SHORT:
        {
            if (!officecfg::Office::Common::AccessibilityIssues::HyperlinkShort::get())
                pIssue->setHidden(true);
        }
        break;
        case sfx::AccessibilityIssueID::HYPERLINK_NO_NAME:
        {
            if (!officecfg::Office::Common::AccessibilityIssues::HyperlinkNoName::get())
                pIssue->setHidden(true);
        }
        break;
        case sfx::AccessibilityIssueID::LINK_IN_HEADER_FOOTER:
        {
            if (!officecfg::Office::Common::AccessibilityIssues::LinkInHeaderOrFooter::get())
                pIssue->setHidden(true);
        }
        break;
        case sfx::AccessibilityIssueID::FAKE_FOOTNOTE:
        {
            if (!officecfg::Office::Common::AccessibilityIssues::FakeFootnotes::get())
                pIssue->setHidden(true);
        }
        break;
        case sfx::AccessibilityIssueID::FAKE_CAPTION:
        {
            if (!officecfg::Office::Common::AccessibilityIssues::FakeCaptions::get())
                pIssue->setHidden(true);
        }
        break;
        case sfx::AccessibilityIssueID::MANUAL_NUMBERING:
        {
            if (!officecfg::Office::Common::AccessibilityIssues::ManualNumbering::get())
                pIssue->setHidden(true);
        }
        break;

        case sfx::AccessibilityIssueID::TEXT_CONTRAST:
        {
            if (!officecfg::Office::Common::AccessibilityIssues::TextContrast::get())
                pIssue->setHidden(true);
        }
        break;
        case sfx::AccessibilityIssueID::TEXT_BLINKING:
        {
            if (!officecfg::Office::Common::AccessibilityIssues::TextBlinking::get())
                pIssue->setHidden(true);
        }
        break;
        case sfx::AccessibilityIssueID::HEADINGS_NOT_IN_ORDER:
        {
            if (!officecfg::Office::Common::AccessibilityIssues::HeadingNotInOrder::get())
                pIssue->setHidden(true);
        }
        break;
        case sfx::AccessibilityIssueID::NON_INTERACTIVE_FORMS:
        {
            if (!officecfg::Office::Common::AccessibilityIssues::NonInteractiveForms::get())
                pIssue->setHidden(true);
        }
        break;
        case sfx::AccessibilityIssueID::FLOATING_TEXT:
        {
            if (!officecfg::Office::Common::AccessibilityIssues::Floatingtext::get())
                pIssue->setHidden(true);
        }
        break;
        case sfx::AccessibilityIssueID::HEADING_IN_TABLE:
        {
            if (!officecfg::Office::Common::AccessibilityIssues::HeadingTable::get())
                pIssue->setHidden(true);
        }
        break;
        case sfx::AccessibilityIssueID::HEADING_START:
        {
            if (!officecfg::Office::Common::AccessibilityIssues::HeadingStart::get())
                pIssue->setHidden(true);
        }
        break;
        case sfx::AccessibilityIssueID::HEADING_ORDER:
        {
            if (!officecfg::Office::Common::AccessibilityIssues::HeadingOrder::get())
                pIssue->setHidden(true);
        }
        break;
        case sfx::AccessibilityIssueID::CONTENT_CONTROL:
        {
            if (!officecfg::Office::Common::AccessibilityIssues::ContentControl::get())
                pIssue->setHidden(true);
        }
        break;
        case sfx::AccessibilityIssueID::AVOID_FOOTNOTES:
        {
            if (!officecfg::Office::Common::AccessibilityIssues::AvoidFootnotes::get())
                pIssue->setHidden(true);
        }
        break;
        case sfx::AccessibilityIssueID::AVOID_ENDNOTES:
        {
            if (!officecfg::Office::Common::AccessibilityIssues::AvoidEndnotes::get())
                pIssue->setHidden(true);
        }
        break;
        case sfx::AccessibilityIssueID::FONTWORKS:
        {
            if (!officecfg::Office::Common::AccessibilityIssues::FontWorks::get())
                pIssue->setHidden(true);
        }
        break;
        default:
        {
            SAL_WARN("sw.a11y", "Invalid issue ID.");
            break;
        }
    }
}

std::shared_ptr<sw::AccessibilityIssue>
lclAddIssue(sfx::AccessibilityIssueCollection& rIssueCollection, OUString const& rText,
            sfx::AccessibilityIssueID eIssueId, sfx::AccessibilityIssueLevel eIssueLvl)
{
    auto pIssue = std::make_shared<sw::AccessibilityIssue>(eIssueId, eIssueLvl);
    pIssue->m_aIssueText = rText;

    // check which a11y issue should be visible
    lcl_SetHiddenIssues(pIssue);

    rIssueCollection.getIssues().push_back(pIssue);
    return pIssue;
}

class NodeCheck : public BaseCheck
{
public:
    NodeCheck(sfx::AccessibilityIssueCollection& rIssueCollection)
        : BaseCheck(rIssueCollection)
    {
    }

    virtual void check(SwNode* pCurrent) = 0;
};

// Check NoTextNodes: Graphic, OLE for alt (title) text
class NoTextNodeAltTextCheck : public NodeCheck
{
    void checkNoTextNode(SwNoTextNode* pNoTextNode)
    {
        if (!pNoTextNode)
            return;

        const SwFrameFormat* pFrameFormat = pNoTextNode->GetFlyFormat();
        if (!pFrameFormat)
            return;

        // linked graphic with broken link
        if (pNoTextNode->IsGrfNode() && pNoTextNode->GetGrfNode()->IsLinkedFile())
        {
            OUString sURL(pNoTextNode->GetGrfNode()->GetGraphic().getOriginURL());
            if (!FStatHelper::IsDocument(sURL))
            {
                INetURLObject aURL(sURL);
                OUString aSystemPath = sURL;

                // abbreviate URL
                if (aURL.GetProtocol() == INetProtocol::File)
                {
                    OUString aAbbreviatedPath;
                    aSystemPath = aURL.getFSysPath(FSysStyle::Detect);
                    osl_abbreviateSystemPath(aSystemPath.pData, &aAbbreviatedPath.pData, 46,
                                             nullptr);
                    sURL = aAbbreviatedPath;
                }

                OUString sIssueText
                    = SwResId(STR_LINKED_GRAPHIC)
                          .replaceAll("%OBJECT_NAME%", pFrameFormat->GetName().toString())
                          .replaceFirst("%LINK%", sURL);

                auto pIssue = lclAddIssue(m_rIssueCollection, sIssueText,
                                          sfx::AccessibilityIssueID::LINKED_GRAPHIC,
                                          sfx::AccessibilityIssueLevel::WARNLEV);
                pIssue->setDoc(pNoTextNode->GetDoc());
                pIssue->setIssueObject(IssueObject::LINKED);
                pIssue->setObjectID(pFrameFormat->GetName().toString());
                pIssue->setNode(pNoTextNode);
                pIssue->setAdditionalInfo({ aSystemPath });
            }
        }

        if (!pNoTextNode->GetTitle().isEmpty() || !pNoTextNode->GetDescription().isEmpty())
            return;

        OUString sIssueText
            = SwResId(STR_NO_ALT).replaceAll("%OBJECT_NAME%", pFrameFormat->GetName().toString());

        if (pNoTextNode->IsOLENode())
        {
            auto pIssue
                = lclAddIssue(m_rIssueCollection, sIssueText, sfx::AccessibilityIssueID::NO_ALT_OLE,
                              sfx::AccessibilityIssueLevel::ERRORLEV);
            pIssue->setDoc(pNoTextNode->GetDoc());
            pIssue->setIssueObject(IssueObject::OLE);
            pIssue->setObjectID(pFrameFormat->GetName().toString());
        }
        else if (pNoTextNode->IsGrfNode())
        {
            const SfxBoolItem* pIsDecorItem = pFrameFormat->GetItemIfSet(RES_DECORATIVE);
            if (!(pIsDecorItem && pIsDecorItem->GetValue()))
            {
                auto pIssue = lclAddIssue(m_rIssueCollection, sIssueText,
                                          sfx::AccessibilityIssueID::NO_ALT_GRAPHIC,
                                          sfx::AccessibilityIssueLevel::ERRORLEV);
                pIssue->setDoc(pNoTextNode->GetDoc());
                pIssue->setIssueObject(IssueObject::GRAPHIC);
                pIssue->setObjectID(pFrameFormat->GetName().toString());
                pIssue->setNode(pNoTextNode);
            }
        }
    }

public:
    NoTextNodeAltTextCheck(sfx::AccessibilityIssueCollection& rIssueCollection)
        : NodeCheck(rIssueCollection)
    {
    }

    void check(SwNode* pCurrent) override
    {
        if (pCurrent->GetNodeType() & SwNodeType::NoTextMask)
        {
            SwNoTextNode* pNoTextNode = pCurrent->GetNoTextNode();
            if (pNoTextNode)
                checkNoTextNode(pNoTextNode);
        }
    }
};

// Check Table node if the table is merged and split.
class TableNodeMergeSplitCheck : public NodeCheck
{
private:
    void addTableIssue(SwTable const& rTable, SwDoc& rDoc)
    {
        const SwTableFormat* pFormat = rTable.GetFrameFormat();
        UIName sName = pFormat->GetName();
        OUString sIssueText
            = SwResId(STR_TABLE_MERGE_SPLIT).replaceAll("%OBJECT_NAME%", sName.toString());
        auto pIssue = lclAddIssue(m_rIssueCollection, sIssueText,
                                  sfx::AccessibilityIssueID::TABLE_MERGE_SPLIT,
                                  sfx::AccessibilityIssueLevel::WARNLEV);
        pIssue->setDoc(rDoc);
        pIssue->setIssueObject(IssueObject::TABLE);
        pIssue->setObjectID(sName.toString());
    }

    void checkTableNode(SwTableNode* pTableNode)
    {
        if (!pTableNode)
            return;

        SwTable const& rTable = pTableNode->GetTable();
        SwDoc& rDoc = pTableNode->GetDoc();
        if (rTable.IsTableComplex())
        {
            addTableIssue(rTable, rDoc);
        }
        else
        {
            if (rTable.GetTabLines().size() > 1)
            {
                int i = 0;
                size_t nFirstLineSize = 0;
                bool bAllColumnsSameSize = true;
                bool bCellSpansOverMoreRows = false;

                for (SwTableLine const* pTableLine : rTable.GetTabLines())
                {
                    if (i == 0)
                    {
                        nFirstLineSize = pTableLine->GetTabBoxes().size();
                    }
                    else
                    {
                        size_t nLineSize = pTableLine->GetTabBoxes().size();
                        if (nFirstLineSize != nLineSize)
                        {
                            bAllColumnsSameSize = false;
                        }
                    }
                    i++;

                    // Check for row span in each table box (cell)
                    for (SwTableBox const* pBox : pTableLine->GetTabBoxes())
                    {
                        if (pBox->getRowSpan() > 1)
                            bCellSpansOverMoreRows = true;
                    }
                }
                if (!bAllColumnsSameSize || bCellSpansOverMoreRows)
                {
                    addTableIssue(rTable, rDoc);
                }
            }
        }
    }

public:
    TableNodeMergeSplitCheck(sfx::AccessibilityIssueCollection& rIssueCollection)
        : NodeCheck(rIssueCollection)
    {
    }

    void check(SwNode* pCurrent) override
    {
        if (pCurrent->GetNodeType() & SwNodeType::Table)
        {
            SwTableNode* pTableNode = pCurrent->GetTableNode();
            if (pTableNode)
                checkTableNode(pTableNode);
        }
    }
};

class TableFormattingCheck : public NodeCheck
{
private:
    void checkTableNode(SwTableNode* pTableNode)
    {
        if (!pTableNode)
            return;

        const SwTable& rTable = pTableNode->GetTable();
        if (!rTable.IsTableComplex())
        {
            size_t nEmptyBoxes = 0;
            size_t nBoxCount = 0;
            for (const SwTableLine* pTableLine : rTable.GetTabLines())
            {
                nBoxCount += pTableLine->GetTabBoxes().size();
                for (const SwTableBox* pBox : pTableLine->GetTabBoxes())
                    if (pBox->IsEmpty())
                        ++nEmptyBoxes;
            }
            // If more than half of the boxes are empty we can assume that it is used for formatting
            if (nEmptyBoxes > nBoxCount / 2)
            {
                auto pIssue = lclAddIssue(m_rIssueCollection, SwResId(STR_TABLE_FORMATTING),
                                          sfx::AccessibilityIssueID::TABLE_FORMATTING,
                                          sfx::AccessibilityIssueLevel::WARNLEV);

                pIssue->setDoc(pTableNode->GetDoc());
                pIssue->setIssueObject(IssueObject::TABLE);
                if (const SwTableFormat* pFormat = rTable.GetFrameFormat())
                    pIssue->setObjectID(pFormat->GetName().toString());
            }
        }
    }

public:
    TableFormattingCheck(sfx::AccessibilityIssueCollection& rIssueCollection)
        : NodeCheck(rIssueCollection)
    {
    }

    void check(SwNode* pCurrent) override
    {
        if (pCurrent->GetNodeType() & SwNodeType::Table)
        {
            SwTableNode* pTableNode = pCurrent->GetTableNode();
            if (pTableNode)
                checkTableNode(pTableNode);
        }
    }
};

class NumberingCheck : public NodeCheck
{
private:
    const std::vector<std::pair<OUString, OUString>> m_aNumberingCombinations{
        { "1.", "2." }, { "(1)", "(2)" }, { "1)", "2)" },   { "a.", "b." }, { "(a)", "(b)" },
        { "a)", "b)" }, { "A.", "B." },   { "(A)", "(B)" }, { "A)", "B)" }
    };

public:
    NumberingCheck(sfx::AccessibilityIssueCollection& rIssueCollection)
        : NodeCheck(rIssueCollection)
    {
    }

    void check(SwNode* pCurrent) override
    {
        if (!pCurrent->IsTextNode())
            return;

        SwTextNode* pCurrentTextNode = pCurrent->GetTextNode();
        SwTextNode* pNextTextNode = lclSearchNextTextNode(pCurrent);

        if (!pNextTextNode)
            return;

        SwSectionNode* pNd = pCurrentTextNode->FindSectionNode();
        if (pNd && pNd->GetSection().GetType() == SectionType::ToxContent)
            return;

        for (auto& rPair : m_aNumberingCombinations)
        {
            if (pCurrentTextNode->GetText().startsWith(rPair.first)
                && pNextTextNode->GetText().startsWith(rPair.second))
            {
                OUString sNumbering = rPair.first + " " + rPair.second + "...";
                OUString sIssueText
                    = SwResId(STR_FAKE_NUMBERING).replaceAll("%NUMBERING%", sNumbering);
                auto pIssue = lclAddIssue(m_rIssueCollection, sIssueText,
                                          sfx::AccessibilityIssueID::MANUAL_NUMBERING,
                                          sfx::AccessibilityIssueLevel::WARNLEV);
                pIssue->setIssueObject(IssueObject::TEXT);
                pIssue->setDoc(pCurrent->GetDoc());
                pIssue->setNode(pCurrent);
            }
        }
    }
};

class HyperlinkCheck : public NodeCheck
{
private:
    void checkHyperLinks(SwTextNode* pTextNode)
    {
        const OUString& sParagraphText = pTextNode->GetText();
        SwpHints& rHints = pTextNode->GetSwpHints();
        for (size_t i = 0; i < rHints.Count(); ++i)
        {
            const SwTextAttr* pTextAttr = rHints.Get(i);
            SwDoc& rDocument = pTextNode->GetDoc();
            if (pTextAttr->Which() == RES_TXTATR_INETFMT)
            {
                OUString sHyperlink = pTextAttr->GetINetFormat().GetValue();
                if (!sHyperlink.isEmpty())
                {
                    INetURLObject aHyperlink(sHyperlink);
                    std::shared_ptr<sw::AccessibilityIssue> pIssue;
                    sal_Int32 nStart = pTextAttr->GetStart();
                    OUString sRunText = sParagraphText.copy(nStart, *pTextAttr->GetEnd() - nStart);

                    if (aHyperlink.GetProtocol() != INetProtocol::NotValid
                        && INetURLObject(sRunText) == aHyperlink)
                    {
                        OUString sIssueText = SwResId(STR_HYPERLINK_TEXT_IS_LINK)
                                                  .replaceFirst("%LINK%", sHyperlink);
                        pIssue = lclAddIssue(m_rIssueCollection, sIssueText,
                                             sfx::AccessibilityIssueID::HYPERLINK_IS_TEXT,
                                             sfx::AccessibilityIssueLevel::WARNLEV);
                    }
                    else if (sRunText.getLength() <= 5)
                    {
                        pIssue
                            = lclAddIssue(m_rIssueCollection, SwResId(STR_HYPERLINK_TEXT_IS_SHORT),
                                          sfx::AccessibilityIssueID::HYPERLINK_SHORT,
                                          sfx::AccessibilityIssueLevel::WARNLEV);
                    }

                    if (pIssue)
                    {
                        pIssue->setIssueObject(IssueObject::TEXT);
                        pIssue->setNode(pTextNode);
                        pIssue->setDoc(rDocument);
                        pIssue->setStart(nStart);
                        pIssue->setEnd(nStart + sRunText.getLength());
                    }

                    if (aHyperlink.GetProtocol() != INetProtocol::NotValid)
                    {
                        OUString sHyperlinkName = pTextAttr->GetINetFormat().GetName();
                        if (sHyperlinkName.isEmpty())
                        {
                            pIssue = lclAddIssue(m_rIssueCollection, SwResId(STR_HYPERLINK_NO_NAME),
                                                 sfx::AccessibilityIssueID::HYPERLINK_NO_NAME,
                                                 sfx::AccessibilityIssueLevel::WARNLEV);

                            if (pIssue)
                            {
                                pIssue->setIssueObject(IssueObject::HYPERLINKTEXT);
                                pIssue->setNode(pTextNode);
                                pIssue->setDoc(rDocument);
                                pIssue->setStart(nStart);
                                pIssue->setEnd(nStart + sRunText.getLength());
                            }
                        }
                    }

                    // check Hyperlinks in Header/Footer --> annotation is not nested
                    if (rDocument.IsInHeaderFooter(*pTextNode))
                    {
                        pIssue
                            = lclAddIssue(m_rIssueCollection, SwResId(STR_LINK_TEXT_IS_NOT_NESTED),
                                          sfx::AccessibilityIssueID::LINK_IN_HEADER_FOOTER,
                                          sfx::AccessibilityIssueLevel::WARNLEV);

                        if (pIssue)
                        {
                            pIssue->setIssueObject(IssueObject::TEXT);
                            pIssue->setNode(pTextNode);
                            pIssue->setDoc(rDocument);
                            pIssue->setStart(nStart);
                            pIssue->setEnd(nStart + sRunText.getLength());
                        }
                    }
                }
            }

            // check other Link's in Header/Footer --> annotation is not nested
            if (pTextAttr->Which() == RES_TXTATR_FIELD && rDocument.IsInHeaderFooter(*pTextNode))
            {
                bool bWarning = false;
                const SwField* pField = pTextAttr->GetFormatField().GetField();
                if (SwFieldIds::GetRef == pField->Which())
                {
                    bWarning = true;
                }
                else if (SwFieldIds::TableOfAuthorities == pField->Which())
                {
                    const auto& rAuthorityField = *static_cast<const SwAuthorityField*>(pField);
                    if (auto targetType = rAuthorityField.GetTargetType();
                        targetType == SwAuthorityField::TargetType::None)
                    {
                        bWarning = false;
                    }
                    else
                    {
                        bWarning = true;
                    }
                }

                if (bWarning)
                {
                    auto pIssue
                        = lclAddIssue(m_rIssueCollection, SwResId(STR_LINK_TEXT_IS_NOT_NESTED),
                                      sfx::AccessibilityIssueID::LINK_IN_HEADER_FOOTER,
                                      sfx::AccessibilityIssueLevel::WARNLEV);

                    if (pIssue)
                    {
                        sal_Int32 nStart = pTextAttr->GetStart();
                        pIssue->setIssueObject(IssueObject::TEXT);
                        pIssue->setNode(pTextNode);
                        pIssue->setDoc(rDocument);
                        pIssue->setStart(nStart);
                        pIssue->setEnd(nStart + 1);
                    }
                }
            }
        }
    }

public:
    HyperlinkCheck(sfx::AccessibilityIssueCollection& rIssueCollection)
        : NodeCheck(rIssueCollection)
    {
    }

    void check(SwNode* pCurrent) override
    {
        if (!pCurrent->IsTextNode())
            return;

        SwTextNode* pTextNode = pCurrent->GetTextNode();
        if (pTextNode->HasHints())
        {
            checkHyperLinks(pTextNode);
        }
    }
};

// Based on https://www.w3.org/TR/WCAG21/#dfn-relative-luminance
double calculateRelativeLuminance(Color const& rColor)
{
    // Convert to BColor which has R, G, B colors components
    // represented by a floating point number from [0.0, 1.0]
    const basegfx::BColor aBColor = rColor.getBColor();

    double r = aBColor.getRed();
    double g = aBColor.getGreen();
    double b = aBColor.getBlue();

    // Calculate the values according to the described algorithm
    r = (r <= 0.04045) ? r / 12.92 : std::pow((r + 0.055) / 1.055, 2.4);
    g = (g <= 0.04045) ? g / 12.92 : std::pow((g + 0.055) / 1.055, 2.4);
    b = (b <= 0.04045) ? b / 12.92 : std::pow((b + 0.055) / 1.055, 2.4);

    return 0.2126 * r + 0.7152 * g + 0.0722 * b;
}

// TODO move to common color tools (BColorTools maybe)
// Based on https://www.w3.org/TR/WCAG21/#dfn-contrast-ratio
double calculateContrastRatio(Color const& rColor1, Color const& rColor2)
{
    const double fLuminance1 = calculateRelativeLuminance(rColor1);
    const double fLuminance2 = calculateRelativeLuminance(rColor2);
    const std::pair<const double, const double> aMinMax = std::minmax(fLuminance1, fLuminance2);

    // (L1 + 0.05) / (L2 + 0.05)
    // L1 is the lighter color (greater luminance value)
    // L2 is the darker color (smaller luminance value)
    return (aMinMax.second + 0.05) / (aMinMax.first + 0.05);
}

// Determine required minimum contrast ratio for text with the given properties
// according to https://www.w3.org/WAI/WCAG22/Understanding/contrast-minimum.html
// * 3.0 for large text (font size >= 18 or bold and font size >= 14)
// * 4.5 otherwise
double minimumContrastRatio(const uno::Reference<beans::XPropertySet>& xProperties)
{
    double fMinimumContrastRatio = 4.5;
    double fFontSize = 0;
    if (xProperties->getPropertyValue(u"CharHeight"_ustr) >>= fFontSize)
    {
        if (fFontSize >= 18)
            fMinimumContrastRatio = 3.0;
        else if (fFontSize >= 14)
        {
            double fCharWeight = 0;
            if (xProperties->getPropertyValue(u"CharWeight"_ustr) >>= fCharWeight)
            {
                if (fCharWeight == css::awt::FontWeight::BOLD
                    || fCharWeight == css::awt::FontWeight::ULTRABOLD)
                    fMinimumContrastRatio = 3.0;
            }
        }
    }
    return fMinimumContrastRatio;
}

class TextContrastCheck : public NodeCheck
{
private:
    void checkTextRange(uno::Reference<text::XTextRange> const& xTextRange,
                        uno::Reference<text::XTextContent> const& xParagraph, SwTextNode* pTextNode,
                        sal_Int32 nTextStart)
    {
        if (xTextRange->getString().isEmpty())
            return;

        Color nParaBackColor(COL_AUTO);
        uno::Reference<beans::XPropertySet> xParagraphProperties(xParagraph, uno::UNO_QUERY);
        if (!(xParagraphProperties->getPropertyValue(u"ParaBackColor"_ustr) >>= nParaBackColor))
        {
            SAL_WARN("sw.a11y", "ParaBackColor void");
            return;
        }

        uno::Reference<beans::XPropertySet> xProperties(xTextRange, uno::UNO_QUERY);
        if (!xProperties.is())
            return;

        // Foreground color
        sal_Int32 nCharColor = {}; // spurious -Werror=maybe-uninitialized
        if (!(xProperties->getPropertyValue(u"CharColor"_ustr) >>= nCharColor))
        { // not sure this is impossible, can the default be void?
            SAL_WARN("sw.a11y", "CharColor void");
            return;
        }

        const SwPageDesc* pPageDescription = pTextNode->FindPageDesc();
        if (!pPageDescription)
            return;
        const SwFrameFormat& rPageFormat = pPageDescription->GetMaster();
        const SwAttrSet& rPageSet = rPageFormat.GetAttrSet();

        const XFillStyleItem* pXFillStyleItem(
            rPageSet.GetItem<XFillStyleItem>(XATTR_FILLSTYLE, false));
        Color aPageBackground(COL_AUTO);

        if (pXFillStyleItem && pXFillStyleItem->GetValue() == css::drawing::FillStyle_SOLID)
        {
            const XFillColorItem* rXFillColorItem
                = rPageSet.GetItem<XFillColorItem>(XATTR_FILLCOLOR, false);
            aPageBackground = rXFillColorItem->GetColorValue();
        }

        Color nCharBackColor(COL_AUTO);

        if (!(xProperties->getPropertyValue(u"CharBackColor"_ustr) >>= nCharBackColor))
        {
            SAL_WARN("sw.a11y", "CharBackColor void");
            return;
        }
        // Determine the background color
        // Try Character background (Character highlighting color)
        Color aBackgroundColor(nCharBackColor);

        // If not character background color, try paragraph background color
        if (aBackgroundColor == COL_AUTO)
            aBackgroundColor = nParaBackColor;
        else
        {
            SwDocShell* pDocShell = pTextNode->GetDoc().GetDocShell();
            if (!pDocShell)
                return;

            OUString sCharStyleName;
            Color nCharStyleBackColor(COL_AUTO);
            if (xProperties->getPropertyValue(u"CharStyleName"_ustr) >>= sCharStyleName)
            {
                try
                {
                    uno::Reference<style::XStyleFamiliesSupplier> xStyleFamiliesSupplier(
                        pDocShell->GetModel(), uno::UNO_QUERY);
                    uno::Reference<container::XNameAccess> xCont
                        = xStyleFamiliesSupplier->getStyleFamilies();
                    uno::Reference<container::XNameAccess> xStyleFamily(
                        xCont->getByName(u"CharacterStyles"_ustr), uno::UNO_QUERY);
                    uno::Reference<beans::XPropertySet> xInfo(
                        xStyleFamily->getByName(sCharStyleName), uno::UNO_QUERY);
                    xInfo->getPropertyValue(u"CharBackColor"_ustr) >>= nCharStyleBackColor;
                }
                catch (const uno::Exception&)
                {
                }
            }
            else
            {
                SAL_WARN("sw.a11y", "CharStyleName void");
            }

            if (aBackgroundColor != nCharStyleBackColor)
            {
                auto pIssue
                    = lclAddIssue(m_rIssueCollection, SwResId(STR_TEXT_FORMATTING_CONVEYS_MEANING),
                                  sfx::AccessibilityIssueID::DIRECT_FORMATTING,
                                  sfx::AccessibilityIssueLevel::WARNLEV);
                pIssue->setIssueObject(IssueObject::TEXT);
                pIssue->setNode(pTextNode);
                SwDoc& rDocument = pTextNode->GetDoc();
                pIssue->setDoc(rDocument);
                pIssue->setStart(nTextStart);
                pIssue->setEnd(nTextStart + xTextRange->getString().getLength());
            }
        }

        Color aForegroundColor(ColorTransparency, nCharColor);
        if (aForegroundColor == COL_AUTO)
            return;

        // If not paragraph background color, try page color
        if (aBackgroundColor == COL_AUTO)
            aBackgroundColor = aPageBackground;

        // If not page color, assume white background color
        if (aBackgroundColor == COL_AUTO)
            aBackgroundColor = COL_WHITE;

        double fContrastRatio = calculateContrastRatio(aForegroundColor, aBackgroundColor);
        if (fContrastRatio < minimumContrastRatio(xProperties))
        {
            auto pIssue = lclAddIssue(m_rIssueCollection, SwResId(STR_TEXT_CONTRAST),
                                      sfx::AccessibilityIssueID::TEXT_CONTRAST,
                                      sfx::AccessibilityIssueLevel::WARNLEV);
            pIssue->setIssueObject(IssueObject::TEXT);
            pIssue->setNode(pTextNode);
            pIssue->setDoc(pTextNode->GetDoc());
            pIssue->setStart(nTextStart);
            pIssue->setEnd(nTextStart + xTextRange->getString().getLength());
        }
    }

public:
    TextContrastCheck(sfx::AccessibilityIssueCollection& rIssueCollection)
        : NodeCheck(rIssueCollection)
    {
    }

    void check(SwNode* pCurrent) override
    {
        if (!pCurrent->IsTextNode())
            return;

        SwTextNode* pTextNode = pCurrent->GetTextNode();
        rtl::Reference<SwXParagraph> xParagraph
            = SwXParagraph::CreateXParagraph(pTextNode->GetDoc(), pTextNode, nullptr);
        if (!xParagraph.is())
            return;

        uno::Reference<container::XEnumeration> xRunEnum = xParagraph->createEnumeration();
        sal_Int32 nStart = 0;
        while (xRunEnum->hasMoreElements())
        {
            uno::Reference<text::XTextRange> xRun(xRunEnum->nextElement(), uno::UNO_QUERY);
            if (xRun.is())
            {
                checkTextRange(xRun, xParagraph, pTextNode, nStart);
                nStart += xRun->getString().getLength();
            }
        }
    }
};

class TextFormattingCheck : public NodeCheck
{
public:
    TextFormattingCheck(sfx::AccessibilityIssueCollection& rIssueCollection)
        : NodeCheck(rIssueCollection)
    {
    }

    void checkAutoFormat(SwTextNode* pTextNode, const SwTextAttr* pTextAttr,
                         const std::map<sal_Int32, const SwTextAttr*>& rCharFormats)
    {
        const SwFormatAutoFormat& rAutoFormat = pTextAttr->GetAutoFormat();
        SfxItemIter aItemIter(*rAutoFormat.GetStyleHandle());
        const SfxPoolItem* pItem = aItemIter.GetCurItem();

        const SwTextAttr* pCharAttr = nullptr;
        auto itr = rCharFormats.find(pTextAttr->GetStart());
        if (itr != rCharFormats.end())
            pCharAttr = itr->second;

        const SwCharFormat* pCharformat = nullptr;
        if (pCharAttr && (*pTextAttr->GetEnd() == *pCharAttr->GetEnd()))
            pCharformat = pCharAttr->GetCharFormat().GetCharFormat();

        std::vector<OUString> aFormattings;
        while (pItem)
        {
            OUString sFormattingType;
            switch (pItem->Which())
            {
                case RES_CHRATR_WEIGHT:
                case RES_CHRATR_CJK_WEIGHT:
                case RES_CHRATR_CTL_WEIGHT:
                {
                    const SvxWeightItem* pStyleItem = nullptr;

                    if (pCharformat)
                    {
                        pStyleItem = pCharformat->GetItemIfSet(
                            TypedWhichId<SvxWeightItem>(pItem->Which()), false);
                    }

                    if (!pStyleItem && pTextNode->GetTextColl())
                    {
                        pStyleItem = pTextNode->GetTextColl()->GetItemIfSet(
                            TypedWhichId<SvxWeightItem>(pItem->Which()), false);
                    }

                    if (!pStyleItem)
                    {
                        pStyleItem = &pTextNode->GetDoc().GetAttrPool().GetUserOrPoolDefaultItem(
                            TypedWhichId<SvxWeightItem>(pItem->Which()));
                    }

                    if (!SfxPoolItem::areSame(static_cast<const SvxWeightItem*>(pItem), pStyleItem))
                        sFormattingType = "Weight";
                }
                break;

                case RES_CHRATR_POSTURE:
                case RES_CHRATR_CJK_POSTURE:
                case RES_CHRATR_CTL_POSTURE:
                {
                    const SvxPostureItem* pStyleItem = nullptr;

                    if (pCharformat)
                    {
                        pStyleItem = pCharformat->GetItemIfSet(
                            TypedWhichId<SvxPostureItem>(pItem->Which()), false);
                    }

                    if (!pStyleItem && pTextNode->GetTextColl())
                    {
                        pStyleItem = pTextNode->GetTextColl()->GetItemIfSet(
                            TypedWhichId<SvxPostureItem>(pItem->Which()), false);
                    }

                    if (!pStyleItem)
                    {
                        pStyleItem = &pTextNode->GetDoc().GetAttrPool().GetUserOrPoolDefaultItem(
                            TypedWhichId<SvxPostureItem>(pItem->Which()));
                    }

                    if (!SfxPoolItem::areSame(static_cast<const SvxPostureItem*>(pItem),
                                              pStyleItem))
                    {
                        sFormattingType = "Posture";
                    }
                }
                break;

                case RES_CHRATR_SHADOWED:
                {
                    const SvxShadowedItem* pStyleItem = nullptr;

                    if (pCharformat)
                        pStyleItem = pCharformat->GetItemIfSet(RES_CHRATR_SHADOWED, false);

                    if (!pStyleItem && pTextNode->GetTextColl())
                    {
                        pStyleItem
                            = pTextNode->GetTextColl()->GetItemIfSet(RES_CHRATR_SHADOWED, false);
                    }

                    if (!pStyleItem)
                    {
                        pStyleItem = &pTextNode->GetDoc().GetAttrPool().GetUserOrPoolDefaultItem(
                            RES_CHRATR_SHADOWED);
                    }

                    if (!SfxPoolItem::areSame(static_cast<const SvxShadowedItem*>(pItem),
                                              pStyleItem))
                    {
                        sFormattingType = "Shadowed";
                    }
                }
                break;

                case RES_CHRATR_COLOR:
                {
                    const SvxColorItem* pStyleItem = nullptr;

                    if (pCharformat)
                        pStyleItem = pCharformat->GetItemIfSet(RES_CHRATR_COLOR, false);

                    if (!pStyleItem && pTextNode->GetTextColl())
                    {
                        pStyleItem
                            = pTextNode->GetTextColl()->GetItemIfSet(RES_CHRATR_COLOR, false);
                    }

                    if (!pStyleItem)
                    {
                        pStyleItem = &pTextNode->GetDoc().GetAttrPool().GetUserOrPoolDefaultItem(
                            RES_CHRATR_COLOR);
                    }

                    if (!SfxPoolItem::areSame(static_cast<const SvxColorItem*>(pItem), pStyleItem))
                        sFormattingType = "Font Color";
                }
                break;

                case RES_CHRATR_FONTSIZE:
                {
                    // case RES_CHRATR_CJK_FONTSIZE:
                    // case RES_CHRATR_CTL_FONTSIZE:
                    // TODO: check depending on which lang is used Western, Complex, Asia
                    const SvxFontHeightItem* pStyleItem = nullptr;

                    if (pCharformat)
                    {
                        pStyleItem = pCharformat->GetItemIfSet(
                            TypedWhichId<SvxFontHeightItem>(pItem->Which()), false);
                    }

                    if (!pStyleItem && pTextNode->GetTextColl())
                    {
                        pStyleItem = pTextNode->GetTextColl()->GetItemIfSet(
                            TypedWhichId<SvxFontHeightItem>(pItem->Which()), false);
                    }

                    if (!pStyleItem)
                    {
                        pStyleItem = &pTextNode->GetDoc().GetAttrPool().GetUserOrPoolDefaultItem(
                            TypedWhichId<SvxFontHeightItem>(pItem->Which()));
                    }

                    if (!SfxPoolItem::areSame(static_cast<const SvxFontHeightItem*>(pItem),
                                              pStyleItem))
                    {
                        sFormattingType = "Font Size";
                    }
                }
                break;

                case RES_CHRATR_FONT:
                case RES_CHRATR_CJK_FONT:
                case RES_CHRATR_CTL_FONT:
                {
                    // 3. direct formatting
                    const SvxFontItem* pStyleItem = nullptr;

                    if (pCharformat)
                    {
                        pStyleItem = pCharformat->GetItemIfSet(
                            TypedWhichId<SvxFontItem>(pItem->Which()), false);
                    }

                    if (!pStyleItem && pTextNode->GetTextColl())
                    {
                        pStyleItem = pTextNode->GetTextColl()->GetItemIfSet(
                            TypedWhichId<SvxFontItem>(pItem->Which()), false);
                    }

                    if (!pStyleItem)
                    {
                        pStyleItem = &pTextNode->GetDoc().GetAttrPool().GetUserOrPoolDefaultItem(
                            TypedWhichId<SvxFontItem>(pItem->Which()));
                    }

                    if (!SfxPoolItem::areSame(static_cast<const SvxFontItem*>(pItem), pStyleItem))
                        sFormattingType = "Font";
                }
                break;

                case RES_CHRATR_EMPHASIS_MARK:
                {
                    const SvxEmphasisMarkItem* pStyleItem = nullptr;

                    if (pCharformat)
                        pStyleItem = pCharformat->GetItemIfSet(RES_CHRATR_EMPHASIS_MARK, false);

                    if (!pStyleItem && pTextNode->GetTextColl())
                    {
                        pStyleItem = pTextNode->GetTextColl()->GetItemIfSet(
                            RES_CHRATR_EMPHASIS_MARK, false);
                    }

                    if (!pStyleItem)
                    {
                        pStyleItem = &pTextNode->GetDoc().GetAttrPool().GetUserOrPoolDefaultItem(
                            RES_CHRATR_EMPHASIS_MARK);
                    }

                    if (!SfxPoolItem::areSame(static_cast<const SvxEmphasisMarkItem*>(pItem),
                                              pStyleItem))
                    {
                        sFormattingType = "Emphasis Mark";
                    }
                }
                break;

                case RES_CHRATR_UNDERLINE:
                {
                    const SvxUnderlineItem* pStyleItem = nullptr;

                    if (pCharformat)
                        pStyleItem = pCharformat->GetItemIfSet(RES_CHRATR_UNDERLINE, false);

                    if (!pStyleItem && pTextNode->GetTextColl())
                    {
                        pStyleItem
                            = pTextNode->GetTextColl()->GetItemIfSet(RES_CHRATR_UNDERLINE, false);
                    }

                    if (!pStyleItem)
                    {
                        pStyleItem = &pTextNode->GetDoc().GetAttrPool().GetUserOrPoolDefaultItem(
                            RES_CHRATR_UNDERLINE);
                    }

                    if (!SfxPoolItem::areSame(static_cast<const SvxUnderlineItem*>(pItem),
                                              pStyleItem))
                    {
                        sFormattingType = "Underline";
                    }
                }
                break;

                case RES_CHRATR_OVERLINE:
                {
                    const SvxOverlineItem* pStyleItem = nullptr;

                    if (pCharformat)
                        pStyleItem = pCharformat->GetItemIfSet(RES_CHRATR_OVERLINE, false);

                    if (!pStyleItem && pTextNode->GetTextColl())
                    {
                        pStyleItem
                            = pTextNode->GetTextColl()->GetItemIfSet(RES_CHRATR_OVERLINE, false);
                    }

                    if (!pStyleItem)
                    {
                        pStyleItem = &pTextNode->GetDoc().GetAttrPool().GetUserOrPoolDefaultItem(
                            RES_CHRATR_OVERLINE);
                    }

                    if (!SfxPoolItem::areSame(static_cast<const SvxOverlineItem*>(pItem),
                                              pStyleItem))
                    {
                        sFormattingType = "Overline";
                    }
                }
                break;

                case RES_CHRATR_CROSSEDOUT:
                {
                    const SvxCrossedOutItem* pStyleItem = nullptr;

                    if (pCharformat)
                        pStyleItem = pCharformat->GetItemIfSet(RES_CHRATR_CROSSEDOUT, false);

                    if (!pStyleItem && pTextNode->GetTextColl())
                    {
                        pStyleItem
                            = pTextNode->GetTextColl()->GetItemIfSet(RES_CHRATR_CROSSEDOUT, false);
                    }

                    if (!pStyleItem)
                    {
                        pStyleItem = &pTextNode->GetDoc().GetAttrPool().GetUserOrPoolDefaultItem(
                            RES_CHRATR_CROSSEDOUT);
                    }

                    if (!SfxPoolItem::areSame(static_cast<const SvxCrossedOutItem*>(pItem),
                                              pStyleItem))
                    {
                        sFormattingType = "Strikethrough";
                    }
                }
                break;

                case RES_CHRATR_RELIEF:
                {
                    const SvxCharReliefItem* pStyleItem = nullptr;

                    if (pCharformat)
                        pStyleItem = pCharformat->GetItemIfSet(RES_CHRATR_RELIEF, false);

                    if (!pStyleItem && pTextNode->GetTextColl())
                    {
                        pStyleItem
                            = pTextNode->GetTextColl()->GetItemIfSet(RES_CHRATR_RELIEF, false);
                    }

                    if (!pStyleItem)
                    {
                        pStyleItem = &pTextNode->GetDoc().GetAttrPool().GetUserOrPoolDefaultItem(
                            RES_CHRATR_RELIEF);
                    }

                    if (!SfxPoolItem::areSame(static_cast<const SvxCharReliefItem*>(pItem),
                                              pStyleItem))
                    {
                        sFormattingType = "Relief";
                    }
                }
                break;

                case RES_CHRATR_CONTOUR:
                {
                    const SvxContourItem* pStyleItem = nullptr;

                    if (pCharformat)
                        pStyleItem = pCharformat->GetItemIfSet(RES_CHRATR_CONTOUR, false);

                    if (!pStyleItem && pTextNode->GetTextColl())
                    {
                        pStyleItem
                            = pTextNode->GetTextColl()->GetItemIfSet(RES_CHRATR_CONTOUR, false);
                    }

                    if (!pStyleItem)
                    {
                        pStyleItem = &pTextNode->GetDoc().GetAttrPool().GetUserOrPoolDefaultItem(
                            RES_CHRATR_CONTOUR);
                    }

                    if (!SfxPoolItem::areSame(static_cast<const SvxContourItem*>(pItem),
                                              pStyleItem))
                    {
                        sFormattingType = "Outline";
                    }
                }
                break;

                case RES_CHRATR_NOHYPHEN:
                {
                    const SvxNoHyphenItem* pStyleItem = nullptr;

                    if (pCharformat)
                        pStyleItem = pCharformat->GetItemIfSet(RES_CHRATR_NOHYPHEN, false);

                    if (!pStyleItem && pTextNode->GetTextColl())
                    {
                        pStyleItem
                            = pTextNode->GetTextColl()->GetItemIfSet(RES_CHRATR_NOHYPHEN, false);
                    }

                    if (!pStyleItem)
                    {
                        pStyleItem = &pTextNode->GetDoc().GetAttrPool().GetUserOrPoolDefaultItem(
                            RES_CHRATR_NOHYPHEN);
                    }

                    if (!SfxPoolItem::areSame(static_cast<const SvxNoHyphenItem*>(pItem),
                                              pStyleItem))
                    {
                        sFormattingType = "No Hyphenation";
                    }
                }
                break;

                default:
                    break;
            }
            if (!sFormattingType.isEmpty())
                aFormattings.push_back(sFormattingType);
            pItem = aItemIter.NextItem();
        }
        if (aFormattings.empty())
            return;

        o3tl::remove_duplicates(aFormattings);
        auto pIssue = lclAddIssue(m_rIssueCollection, SwResId(STR_TEXT_FORMATTING_CONVEYS_MEANING),
                                  sfx::AccessibilityIssueID::DIRECT_FORMATTING,
                                  sfx::AccessibilityIssueLevel::WARNLEV);
        pIssue->setIssueObject(IssueObject::TEXT);
        pIssue->setNode(pTextNode);
        SwDoc& rDocument = pTextNode->GetDoc();
        pIssue->setDoc(rDocument);
        pIssue->setStart(pTextAttr->GetStart());
        pIssue->setEnd(pTextAttr->GetAnyEnd());
    }

    static bool isDirectFormat(const SwTextNode* pTextNode, const SwAttrSet& rSwAttrSet)
    {
        const SfxPoolItem* pItem = nullptr;
        if ((pItem = rSwAttrSet.GetItem(RES_CHRATR_WEIGHT, false))
            || (pItem = rSwAttrSet.GetItem(RES_CHRATR_CJK_WEIGHT, false))
            || (pItem = rSwAttrSet.GetItem(RES_CHRATR_CTL_WEIGHT, false)))
        {
            // 3. direct formatting
            const SvxWeightItem* pStyleItem = nullptr;

            if (pTextNode->GetTextColl())
            {
                // 1. paragraph format
                pStyleItem = pTextNode->GetTextColl()->GetItemIfSet(
                    TypedWhichId<SvxWeightItem>(pItem->Which()), false);
            }

            if (!pStyleItem)
            {
                // 0. document default
                pStyleItem = &pTextNode->GetDoc().GetAttrPool().GetUserOrPoolDefaultItem(
                    TypedWhichId<SvxWeightItem>(pItem->Which()));
            }

            if (!SfxPoolItem::areSame(static_cast<const SvxWeightItem*>(pItem), pStyleItem))
                return true;
            else
                pItem = nullptr;
        }

        if ((pItem = rSwAttrSet.GetItem(RES_CHRATR_POSTURE, false))
            || (pItem = rSwAttrSet.GetItem(RES_CHRATR_CJK_POSTURE, false))
            || (pItem = rSwAttrSet.GetItem(RES_CHRATR_CTL_POSTURE, false)))
        {
            const SvxPostureItem* pStyleItem = nullptr;

            if (pTextNode->GetTextColl())
            {
                pStyleItem = pTextNode->GetTextColl()->GetItemIfSet(
                    TypedWhichId<SvxPostureItem>(pItem->Which()), false);
            }

            if (!pStyleItem)
            {
                pStyleItem = &pTextNode->GetDoc().GetAttrPool().GetUserOrPoolDefaultItem(
                    TypedWhichId<SvxPostureItem>(pItem->Which()));
            }

            if (!SfxPoolItem::areSame(static_cast<const SvxPostureItem*>(pItem), pStyleItem))
                return true;
            else
                pItem = nullptr;
        }

        if ((pItem = rSwAttrSet.GetItem(RES_CHRATR_SHADOWED, false)))
        {
            const SvxShadowedItem* pStyleItem = nullptr;

            if (pTextNode->GetTextColl())
                pStyleItem = pTextNode->GetTextColl()->GetItemIfSet(RES_CHRATR_SHADOWED, false);

            if (!pStyleItem)
            {
                pStyleItem = &pTextNode->GetDoc().GetAttrPool().GetUserOrPoolDefaultItem(
                    RES_CHRATR_SHADOWED);
            }

            if (!SfxPoolItem::areSame(static_cast<const SvxShadowedItem*>(pItem), pStyleItem))
                return true;
            else
                pItem = nullptr;
        }

        if ((pItem = rSwAttrSet.GetItem(RES_CHRATR_COLOR, false)))
        {
            const SvxColorItem* pStyleItem = nullptr;

            if (pTextNode->GetTextColl())
                pStyleItem = pTextNode->GetTextColl()->GetItemIfSet(RES_CHRATR_COLOR, false);

            if (!pStyleItem)
            {
                pStyleItem
                    = &pTextNode->GetDoc().GetAttrPool().GetUserOrPoolDefaultItem(RES_CHRATR_COLOR);
            }

            if (!SfxPoolItem::areSame(static_cast<const SvxColorItem*>(pItem), pStyleItem))
                return true;
            else
                pItem = nullptr;
        }

        // TODO: check depending on which lang is used Western, Complex, Asia
        if ((pItem = rSwAttrSet.GetItem(RES_CHRATR_FONTSIZE, false))
            /*|| (pItem = rSwAttrSet.GetItem(RES_CHRATR_CJK_FONTSIZE, false))
            || (pItem = rSwAttrSet.GetItem(RES_CHRATR_CTL_FONTSIZE, false))*/)
        {
            const SvxFontHeightItem* pStyleItem = nullptr;

            if (pTextNode->GetTextColl())
            {
                pStyleItem = pTextNode->GetTextColl()->GetItemIfSet(
                    TypedWhichId<SvxFontHeightItem>(pItem->Which()), false);
            }

            if (!pStyleItem)
            {
                pStyleItem = &pTextNode->GetDoc().GetAttrPool().GetUserOrPoolDefaultItem(
                    TypedWhichId<SvxFontHeightItem>(pItem->Which()));
            }

            if (!SfxPoolItem::areSame(static_cast<const SvxFontHeightItem*>(pItem), pStyleItem))
                return true;
            else
                pItem = nullptr;
        }

        if ((pItem = rSwAttrSet.GetItem(RES_CHRATR_FONT, false))
            || (pItem = rSwAttrSet.GetItem(RES_CHRATR_CJK_FONT, false))
            || (pItem = rSwAttrSet.GetItem(RES_CHRATR_CTL_FONT, false)))
        {
            const SvxFontItem* pStyleItem = nullptr;

            if (pTextNode->GetTextColl())
            {
                pStyleItem = pTextNode->GetTextColl()->GetItemIfSet(
                    TypedWhichId<SvxFontItem>(pItem->Which()), false);
            }

            if (!pStyleItem)
            {
                pStyleItem = &pTextNode->GetDoc().GetAttrPool().GetUserOrPoolDefaultItem(
                    TypedWhichId<SvxFontItem>(pItem->Which()));
            }

            if (!SfxPoolItem::areSame(static_cast<const SvxFontItem*>(pItem), pStyleItem))
                return true;
            else
                pItem = nullptr;
        }

        if ((pItem = rSwAttrSet.GetItem(RES_CHRATR_EMPHASIS_MARK, false)))
        {
            const SvxEmphasisMarkItem* pStyleItem = nullptr;

            if (pTextNode->GetTextColl())
            {
                pStyleItem
                    = pTextNode->GetTextColl()->GetItemIfSet(RES_CHRATR_EMPHASIS_MARK, false);
            }

            if (!pStyleItem)
            {
                pStyleItem = &pTextNode->GetDoc().GetAttrPool().GetUserOrPoolDefaultItem(
                    RES_CHRATR_EMPHASIS_MARK);
            }

            if (!SfxPoolItem::areSame(static_cast<const SvxEmphasisMarkItem*>(pItem), pStyleItem))
                return true;
            else
                pItem = nullptr;
        }

        if ((pItem = rSwAttrSet.GetItem(RES_CHRATR_UNDERLINE, false)))
        {
            const SvxUnderlineItem* pStyleItem = nullptr;

            if (pTextNode->GetTextColl())
                pStyleItem = pTextNode->GetTextColl()->GetItemIfSet(RES_CHRATR_UNDERLINE, false);

            if (!pStyleItem)
            {
                pStyleItem = &pTextNode->GetDoc().GetAttrPool().GetUserOrPoolDefaultItem(
                    RES_CHRATR_UNDERLINE);
            }

            if (!SfxPoolItem::areSame(static_cast<const SvxUnderlineItem*>(pItem), pStyleItem))
                return true;
            else
                pItem = nullptr;
        }

        if ((pItem = rSwAttrSet.GetItem(RES_CHRATR_OVERLINE, false)))
        {
            const SvxOverlineItem* pStyleItem = nullptr;

            if (pTextNode->GetTextColl())
                pStyleItem = pTextNode->GetTextColl()->GetItemIfSet(RES_CHRATR_OVERLINE, false);

            if (!pStyleItem)
            {
                pStyleItem = &pTextNode->GetDoc().GetAttrPool().GetUserOrPoolDefaultItem(
                    RES_CHRATR_OVERLINE);
            }

            if (!SfxPoolItem::areSame(static_cast<const SvxOverlineItem*>(pItem), pStyleItem))
                return true;
            else
                pItem = nullptr;
        }

        if ((pItem = rSwAttrSet.GetItem(RES_CHRATR_CROSSEDOUT, false)))
        {
            const SvxCrossedOutItem* pStyleItem = nullptr;

            if (pTextNode->GetTextColl())
                pStyleItem = pTextNode->GetTextColl()->GetItemIfSet(RES_CHRATR_CROSSEDOUT, false);

            if (!pStyleItem)
            {
                pStyleItem = &pTextNode->GetDoc().GetAttrPool().GetUserOrPoolDefaultItem(
                    RES_CHRATR_CROSSEDOUT);
            }

            if (!SfxPoolItem::areSame(static_cast<const SvxCrossedOutItem*>(pItem), pStyleItem))
                return true;
            else
                pItem = nullptr;
        }

        if ((pItem = rSwAttrSet.GetItem(RES_CHRATR_RELIEF, false)))
        {
            const SvxCharReliefItem* pStyleItem = nullptr;

            if (pTextNode->GetTextColl())
                pStyleItem = pTextNode->GetTextColl()->GetItemIfSet(RES_CHRATR_RELIEF, false);

            if (!pStyleItem)
            {
                pStyleItem = &pTextNode->GetDoc().GetAttrPool().GetUserOrPoolDefaultItem(
                    RES_CHRATR_RELIEF);
            }

            if (!SfxPoolItem::areSame(static_cast<const SvxCharReliefItem*>(pItem), pStyleItem))
                return true;
            else
                pItem = nullptr;
        }

        if ((pItem = rSwAttrSet.GetItem(RES_CHRATR_NOHYPHEN, false)))
        {
            const SvxNoHyphenItem* pStyleItem = nullptr;

            if (pTextNode->GetTextColl())
                pStyleItem = pTextNode->GetTextColl()->GetItemIfSet(RES_CHRATR_NOHYPHEN, false);

            if (!pStyleItem)
            {
                pStyleItem = &pTextNode->GetDoc().GetAttrPool().GetUserOrPoolDefaultItem(
                    RES_CHRATR_NOHYPHEN);
            }

            if (!SfxPoolItem::areSame(static_cast<const SvxNoHyphenItem*>(pItem), pStyleItem))
                return true;
            else
                pItem = nullptr;
        }

        if ((pItem = rSwAttrSet.GetItem(RES_CHRATR_CONTOUR, false)))
        {
            const SvxContourItem* pStyleItem = nullptr;

            if (pTextNode->GetTextColl())
                pStyleItem = pTextNode->GetTextColl()->GetItemIfSet(RES_CHRATR_CONTOUR, false);

            if (!pStyleItem)
            {
                pStyleItem = &pTextNode->GetDoc().GetAttrPool().GetUserOrPoolDefaultItem(
                    RES_CHRATR_CONTOUR);
            }

            if (!SfxPoolItem::areSame(static_cast<const SvxContourItem*>(pItem), pStyleItem))
                return true;
            else
                pItem = nullptr;
        }

        return false;
    }

    void check(SwNode* pCurrent) override
    {
        if (!pCurrent->IsTextNode())
            return;

        SwTextNode* pTextNode = pCurrent->GetTextNode();

        SwDocShell* pDocShell = pTextNode->GetDoc().GetDocShell();
        if (!pDocShell)
            return;

        SwWrtShell* pWrtShell = pDocShell->GetWrtShell();
        if (pWrtShell && !pTextNode->getLayoutFrame(pWrtShell->GetLayout()))
            return;

        if (pTextNode->HasHints())
        {
            // collect character style formats (if have)
            SwpHints& rHints = pTextNode->GetSwpHints();
            std::map<sal_Int32, const SwTextAttr*> aCharFormats;
            for (size_t i = 0; i < rHints.Count(); ++i)
            {
                const SwTextAttr* pTextAttr = rHints.Get(i);

                if (pTextAttr->Which() == RES_TXTATR_CHARFMT)
                {
                    aCharFormats.insert({ pTextAttr->GetStart(), pTextAttr });
                }
            }

            // direct formatting
            for (size_t i = 0; i < rHints.Count(); ++i)
            {
                const SwTextAttr* pTextAttr = rHints.Get(i);
                if (pTextAttr->Which() == RES_TXTATR_AUTOFMT)
                {
                    checkAutoFormat(pTextNode, pTextAttr, aCharFormats);
                }
            }
        }

        if (pTextNode->HasSwAttrSet())
        {
            // Paragraph doesn't have hints but the entire paragraph might have char attributes
            auto nParagraphLength = pTextNode->GetText().getLength();
            if (nParagraphLength == 0)
                return;
            if (isDirectFormat(pTextNode, pTextNode->GetSwAttrSet()))
            {
                auto pIssue
                    = lclAddIssue(m_rIssueCollection, SwResId(STR_TEXT_FORMATTING_CONVEYS_MEANING),
                                  sfx::AccessibilityIssueID::DIRECT_FORMATTING,
                                  sfx::AccessibilityIssueLevel::WARNLEV);
                pIssue->setIssueObject(IssueObject::TEXT);
                pIssue->setNode(pTextNode);
                SwDoc& rDocument = pTextNode->GetDoc();
                pIssue->setDoc(rDocument);
                pIssue->setEnd(nParagraphLength);
            }
        }

        // paragraph direct formats (TODO: add more paragraph direct format)
        sal_Int32 nParagraphLength = pTextNode->GetText().getLength();
        if (nParagraphLength != 0 && pTextNode->HasSwAttrSet())
        {
            const SvxULSpaceItem& rULSpace = pTextNode->SwContentNode::GetAttr(RES_UL_SPACE, false);
            bool bULSpace = rULSpace.GetLower() > 0 || rULSpace.GetUpper() > 0;
            if (bULSpace)
            {
                auto pIssue
                    = lclAddIssue(m_rIssueCollection, SwResId(STR_TEXT_FORMATTING_CONVEYS_MEANING),
                                  sfx::AccessibilityIssueID::DIRECT_FORMATTING,
                                  sfx::AccessibilityIssueLevel::WARNLEV);
                pIssue->setIssueObject(IssueObject::TEXT);
                pIssue->setNode(pTextNode);
                SwDoc& rDocument = pTextNode->GetDoc();
                pIssue->setDoc(rDocument);
                pIssue->setEnd(nParagraphLength);
            }
        }
    }
};

class NewlineSpacingCheck : public NodeCheck
{
private:
    static SwTextNode* getPrevTextNode(SwNode* pCurrent)
    {
        SwTextNode* pTextNode = nullptr;

        auto nIndex = pCurrent->GetIndex();

        nIndex--; // go to previous node

        while (pTextNode == nullptr && nIndex >= SwNodeOffset(0))
        {
            auto pNode = pCurrent->GetNodes()[nIndex];
            if (pNode->IsTextNode())
                pTextNode = pNode->GetTextNode();
            nIndex--;
        }

        return pTextNode;
    }

public:
    NewlineSpacingCheck(sfx::AccessibilityIssueCollection& rIssueCollection)
        : NodeCheck(rIssueCollection)
    {
    }
    void check(SwNode* pCurrent) override
    {
        if (!pCurrent->IsTextNode())
            return;

        // Don't count empty table box text nodes
        if (pCurrent->GetTableBox())
            return;

        SwTextNode* pTextNode = pCurrent->GetTextNode();
        SwDoc& rDocument = pTextNode->GetDoc();
        SwDocShell* pDocShell = rDocument.GetDocShell();
        if (!pDocShell)
            return;

        SwWrtShell* pWrtShell = pDocShell->GetWrtShell();
        if (!pWrtShell)
            return;

        auto nParagraphLength = pTextNode->GetText().getLength();
        if (nParagraphLength == 0)
        {
            SwTextNode* pPrevTextNode = getPrevTextNode(pCurrent);
            if (!pPrevTextNode)
                return;

            if (pPrevTextNode->getLayoutFrame(pWrtShell->GetLayout()))
            {
                if (pPrevTextNode->GetText().getLength() == 0)
                {
                    auto pIssue = lclAddIssue(m_rIssueCollection, SwResId(STR_AVOID_NEWLINES_SPACE),
                                              sfx::AccessibilityIssueID::TEXT_NEW_LINES,
                                              sfx::AccessibilityIssueLevel::WARNLEV);
                    pIssue->setIssueObject(IssueObject::TEXT);
                    pIssue->setNode(pTextNode);
                    pIssue->setDoc(rDocument);
                }
            }
        }
        else
        {
            if (pTextNode->getLayoutFrame(pWrtShell->GetLayout()))
            {
                // Check for excess lines inside this paragraph
                const OUString& sParagraphText = pTextNode->GetText();
                int nLineCount = 0;
                for (sal_Int32 i = 0; i < nParagraphLength; i++)
                {
                    auto aChar = sParagraphText[i];
                    if (aChar == '\n')
                    {
                        nLineCount++;
                        // Looking for 2 newline characters and above as one can be part of the line
                        // break after a sentence
                        if (nLineCount > 2)
                        {
                            auto pIssue
                                = lclAddIssue(m_rIssueCollection, SwResId(STR_AVOID_NEWLINES_SPACE),
                                              sfx::AccessibilityIssueID::TEXT_NEW_LINES,
                                              sfx::AccessibilityIssueLevel::WARNLEV);
                            pIssue->setIssueObject(IssueObject::TEXT);
                            pIssue->setNode(pTextNode);
                            pIssue->setDoc(rDocument);
                            pIssue->setStart(i);
                            pIssue->setEnd(i);
                        }
                    }
                    // Don't count carriage return as normal character
                    else if (aChar != '\r')
                    {
                        nLineCount = 0;
                    }
                }
            }
        }
    }
};

class SpaceSpacingCheck : public NodeCheck
{
public:
    SpaceSpacingCheck(sfx::AccessibilityIssueCollection& rIssueCollection)
        : NodeCheck(rIssueCollection)
    {
    }
    void check(SwNode* pCurrent) override
    {
        if (!pCurrent->IsTextNode())
            return;
        SwTextNode* pTextNode = pCurrent->GetTextNode();
        auto nParagraphLength = pTextNode->GetText().getLength();
        const OUString& sParagraphText = pTextNode->GetText();
        sal_Int32 nSpaceCount = 0;
        sal_Int32 nSpaceStart = 0;
        sal_Int32 nTabCount = 0;
        bool bNonSpaceFound = false;
        bool bPreviousWasChar = false;
        bool bPreviousWasTab = false;
        for (sal_Int32 i = 0; i < nParagraphLength; i++)
        {
            switch (sParagraphText[i])
            {
                case ' ':
                {
                    if (bNonSpaceFound)
                    {
                        nSpaceCount++;
                        if (nSpaceCount == 2)
                            nSpaceStart = i;
                    }
                    break;
                }
                case '\t':
                {
                    // Don't warn about tabs in ToC
                    auto pSection = SwDoc::GetCurrSection(SwPosition(*pTextNode, 0));
                    if (pSection && pSection->GetTOXBase())
                        continue;

                    // text between tabs or text align at least with two tabs
                    if (bPreviousWasChar || bPreviousWasTab)
                    {
                        ++nTabCount;
                        if (bPreviousWasChar)
                        {
                            bPreviousWasChar = false;
                            bPreviousWasTab = true;
                        }
                        if (nTabCount == 2)
                        {
                            auto pIssue = lclAddIssue(m_rIssueCollection,
                                                      SwResId(STR_AVOID_TABS_FORMATTING),
                                                      sfx::AccessibilityIssueID::TEXT_TABS,
                                                      sfx::AccessibilityIssueLevel::WARNLEV);
                            pIssue->setIssueObject(IssueObject::TEXT);
                            pIssue->setNode(pTextNode);
                            SwDoc& rDocument = pTextNode->GetDoc();
                            pIssue->setDoc(rDocument);
                            pIssue->setStart(0);
                            pIssue->setEnd(nParagraphLength);
                        }
                    }
                    break;
                }
                default:
                {
                    if (nSpaceCount >= 2)
                    {
                        auto pIssue
                            = lclAddIssue(m_rIssueCollection, SwResId(STR_AVOID_SPACES_SPACE),
                                          sfx::AccessibilityIssueID::TEXT_SPACES,
                                          sfx::AccessibilityIssueLevel::WARNLEV);
                        pIssue->setIssueObject(IssueObject::TEXT);
                        pIssue->setNode(pTextNode);
                        SwDoc& rDocument = pTextNode->GetDoc();
                        pIssue->setDoc(rDocument);
                        pIssue->setStart(nSpaceStart);
                        pIssue->setEnd(nSpaceStart + nSpaceCount - 1);
                    }
                    bNonSpaceFound = true;
                    bPreviousWasChar = true;
                    bPreviousWasTab = false;
                    nSpaceCount = 0;
                    break;
                }
            }
        }
    }
};

class FakeFootnoteCheck : public NodeCheck
{
private:
    void checkAutoFormat(SwTextNode* pTextNode, const SwTextAttr* pTextAttr)
    {
        const SwFormatAutoFormat& rAutoFormat = pTextAttr->GetAutoFormat();
        SfxItemIter aItemIter(*rAutoFormat.GetStyleHandle());
        const SfxPoolItem* pItem = aItemIter.GetCurItem();
        while (pItem)
        {
            if (pItem->Which() == RES_CHRATR_ESCAPEMENT)
            {
                auto pEscapementItem = static_cast<const SvxEscapementItem*>(pItem);
                if (pEscapementItem->GetEscapement() == SvxEscapement::Superscript
                    && pTextAttr->GetStart() == 0 && pTextAttr->GetAnyEnd() == 1)
                {
                    auto pIssue = lclAddIssue(m_rIssueCollection, SwResId(STR_AVOID_FAKE_FOOTNOTES),
                                              sfx::AccessibilityIssueID::FAKE_FOOTNOTE,
                                              sfx::AccessibilityIssueLevel::WARNLEV);
                    pIssue->setIssueObject(IssueObject::TEXT);
                    pIssue->setNode(pTextNode);
                    SwDoc& rDocument = pTextNode->GetDoc();
                    pIssue->setDoc(rDocument);
                    pIssue->setStart(0);
                    pIssue->setEnd(pTextNode->GetText().getLength());
                    break;
                }
            }
            pItem = aItemIter.NextItem();
        }
    }

public:
    FakeFootnoteCheck(sfx::AccessibilityIssueCollection& rIssueCollection)
        : NodeCheck(rIssueCollection)
    {
    }
    void check(SwNode* pCurrent) override
    {
        if (!pCurrent->IsTextNode())
            return;
        SwTextNode* pTextNode = pCurrent->GetTextNode();
        if (pTextNode->GetText().getLength() == 0)
            return;

        if (pTextNode->GetText()[0] == '*')
        {
            auto pIssue = lclAddIssue(m_rIssueCollection, SwResId(STR_AVOID_FAKE_FOOTNOTES),
                                      sfx::AccessibilityIssueID::FAKE_FOOTNOTE,
                                      sfx::AccessibilityIssueLevel::WARNLEV);
            pIssue->setIssueObject(IssueObject::TEXT);
            pIssue->setNode(pTextNode);
            SwDoc& rDocument = pTextNode->GetDoc();
            pIssue->setDoc(rDocument);
            pIssue->setStart(0);
            pIssue->setEnd(pTextNode->GetText().getLength());
        }
        else if (pTextNode->HasHints())
        {
            SwpHints& rHints = pTextNode->GetSwpHints();
            for (size_t i = 0; i < rHints.Count(); ++i)
            {
                const SwTextAttr* pTextAttr = rHints.Get(i);
                if (pTextAttr->Which() == RES_TXTATR_AUTOFMT)
                {
                    checkAutoFormat(pTextNode, pTextAttr);
                }
            }
        }
    }
};

class FakeCaptionCheck : public NodeCheck
{
public:
    FakeCaptionCheck(sfx::AccessibilityIssueCollection& rIssueCollection)
        : NodeCheck(rIssueCollection)
    {
    }
    void check(SwNode* pCurrent) override
    {
        if (!pCurrent->IsTextNode())
            return;

        SwTextNode* pTextNode = pCurrent->GetTextNode();
        const OUString& sText = pTextNode->GetText();

        if (sText.getLength() == 0)
            return;

        // Check if it's a real caption
        if (const SwNode* pStartFly = pCurrent->FindFlyStartNode())
        {
            const SwFrameFormat* pFormat = pStartFly->GetFlyFormat();
            if (pFormat)
                return;
        }

        auto aIter = SwIterator<SwTextFrame, SwTextNode, sw::IteratorMode::UnwrapMulti>(*pTextNode);
        auto nCount = 0;
        for (auto aTextFrame = aIter.First(); aTextFrame; aTextFrame = aIter.Next())
        {
            auto aObjects = aTextFrame->GetDrawObjs();
            if (aObjects)
                nCount += aObjects->size();

            if (nCount > 1)
                return;
        }

        // Check that there's exactly 1 image anchored in this node
        if (nCount == 1)
        {
            OString sTemp;
            sText.convertToString(&sTemp, RTL_TEXTENCODING_ASCII_US, 0);
            if (sText.startsWith(SwResId(STR_POOLCOLL_LABEL))
                || sText.startsWith(SwResId(STR_POOLCOLL_LABEL_ABB))
                || sText.startsWith(SwResId(STR_POOLCOLL_LABEL_TABLE))
                || sText.startsWith(SwResId(STR_POOLCOLL_LABEL_FRAME))
                || sText.startsWith(SwResId(STR_POOLCOLL_LABEL_DRAWING))
                || sText.startsWith(SwResId(STR_POOLCOLL_LABEL_FIGURE)))
            {
                auto pIssue = lclAddIssue(m_rIssueCollection, SwResId(STR_AVOID_FAKE_CAPTIONS),
                                          sfx::AccessibilityIssueID::FAKE_CAPTION,
                                          sfx::AccessibilityIssueLevel::WARNLEV);
                pIssue->setIssueObject(IssueObject::TEXT);
                pIssue->setNode(pTextNode);
                SwDoc& rDocument = pTextNode->GetDoc();
                pIssue->setDoc(rDocument);
                pIssue->setStart(0);
                pIssue->setEnd(sText.getLength());
            }
        }
    }
};

class BlinkingTextCheck : public NodeCheck
{
private:
    void checkTextRange(uno::Reference<text::XTextRange> const& xTextRange, SwTextNode* pTextNode,
                        sal_Int32 nStart)
    {
        uno::Reference<beans::XPropertySet> xProperties(xTextRange, uno::UNO_QUERY);
        if (xProperties.is()
            && xProperties->getPropertySetInfo()->hasPropertyByName(u"CharFlash"_ustr))
        {
            bool bBlinking = false;
            xProperties->getPropertyValue(u"CharFlash"_ustr) >>= bBlinking;

            if (bBlinking)
            {
                auto pIssue = lclAddIssue(m_rIssueCollection, SwResId(STR_TEXT_BLINKING),
                                          sfx::AccessibilityIssueID::TEXT_BLINKING,
                                          sfx::AccessibilityIssueLevel::WARNLEV);
                pIssue->setIssueObject(IssueObject::TEXT);
                pIssue->setNode(pTextNode);
                pIssue->setDoc(pTextNode->GetDoc());
                pIssue->setStart(nStart);
                pIssue->setEnd(nStart + xTextRange->getString().getLength());
            }
        }
    }

public:
    BlinkingTextCheck(sfx::AccessibilityIssueCollection& rIssueCollection)
        : NodeCheck(rIssueCollection)
    {
    }

    void check(SwNode* pCurrent) override
    {
        if (!pCurrent->IsTextNode())
            return;

        SwTextNode* pTextNode = pCurrent->GetTextNode();
        rtl::Reference<SwXParagraph> xParagraph
            = SwXParagraph::CreateXParagraph(pTextNode->GetDoc(), pTextNode, nullptr);
        if (!xParagraph.is())
            return;

        uno::Reference<container::XEnumeration> xRunEnum = xParagraph->createEnumeration();
        sal_Int32 nStart = 0;
        while (xRunEnum->hasMoreElements())
        {
            uno::Reference<text::XTextRange> xRun(xRunEnum->nextElement(), uno::UNO_QUERY);
            if (xRun.is())
            {
                checkTextRange(xRun, pTextNode, nStart);
                nStart += xRun->getString().getLength();
            }
        }
    }
};

class HeaderCheck : public NodeCheck
{
private:
    int m_nPreviousLevel;

public:
    HeaderCheck(sfx::AccessibilityIssueCollection& rIssueCollection)
        : NodeCheck(rIssueCollection)
        , m_nPreviousLevel(0)
    {
    }

    void check(SwNode* pCurrent) override
    {
        if (!pCurrent->IsTextNode())
            return;

        SwTextNode* pTextNode = pCurrent->GetTextNode();
        SwTextFormatColl* pCollection = pTextNode->GetTextColl();
        if (!pCollection->IsAssignedToListLevelOfOutlineStyle())
            return;

        int nLevel = pCollection->GetAssignedOutlineStyleLevel();
        assert(nLevel >= 0);
        if (nLevel > m_nPreviousLevel && std::abs(nLevel - m_nPreviousLevel) > 1)
        {
            auto pIssue = lclAddIssue(m_rIssueCollection, SwResId(STR_HEADINGS_NOT_IN_ORDER),
                                      sfx::AccessibilityIssueID::HEADINGS_NOT_IN_ORDER,
                                      sfx::AccessibilityIssueLevel::ERRORLEV);
            pIssue->setIssueObject(IssueObject::TEXT);
            pIssue->setDoc(pCurrent->GetDoc());
            pIssue->setNode(pCurrent);
        }
        m_nPreviousLevel = nLevel;
    }
};

// ISO 142891-1 : 7.14
class NonInteractiveFormCheck : public NodeCheck
{
public:
    NonInteractiveFormCheck(sfx::AccessibilityIssueCollection& rIssueCollection)
        : NodeCheck(rIssueCollection)
    {
    }

    void check(SwNode* pCurrent) override
    {
        if (!pCurrent->IsTextNode())
            return;

        SwTextNode* pTextNode = pCurrent->GetTextNode();
        const auto& text = pTextNode->GetText();

        // Series of tests to detect if there are fake forms in the text.
        bool bCheck = text.indexOf("___") == -1; // Repeated underscores.

        if (bCheck)
            bCheck = text.indexOf("....") == -1; // Repeated dots.

        if (bCheck)
            bCheck = text.indexOf(u"……") == -1; // Repeated ellipsis.

        if (bCheck)
            bCheck = text.indexOf(u"….") == -1; // A dot after an ellipsis.

        if (bCheck)
            bCheck = text.indexOf(u".…") == -1; // An ellipsis after a dot.

        // Checking if all the tests are passed successfully. If not, adding a warning.
        if (!bCheck)
        {
            sal_Int32 nStart = 0;
            auto pIssue = lclAddIssue(m_rIssueCollection, SwResId(STR_NON_INTERACTIVE_FORMS),
                                      sfx::AccessibilityIssueID::NON_INTERACTIVE_FORMS,
                                      sfx::AccessibilityIssueLevel::WARNLEV);
            pIssue->setIssueObject(IssueObject::TEXT);
            pIssue->setNode(pTextNode);
            pIssue->setDoc(pTextNode->GetDoc());
            pIssue->setStart(nStart);
            pIssue->setEnd(nStart + text.getLength());
        }
    }
};

/// Check for floating text frames, as it causes problems with reading order.
class FloatingTextCheck : public NodeCheck
{
public:
    FloatingTextCheck(sfx::AccessibilityIssueCollection& rIssueCollection)
        : NodeCheck(rIssueCollection)
    {
    }

    void check(SwNode* pCurrent) override
    {
        // if node is a text-node and if it has text, we proceed. Otherwise - return.
        const SwTextNode* textNode = pCurrent->GetTextNode();
        if (!textNode || textNode->GetText().isEmpty())
            return;

        // If a node is in fly and if it is not anchored as char, throw warning.
        const SwNode* pStartFly = pCurrent->FindFlyStartNode();
        if (!pStartFly)
            return;

        const SwFrameFormat* pFormat = pStartFly->GetFlyFormat();
        if (!pFormat)
            return;

        if (pFormat->GetAnchor().GetAnchorId() != RndStdIds::FLY_AS_CHAR)
        {
            SwNodeIndex aCurrentIdx(*pCurrent);
            SwNodeIndex aIdx(*pStartFly);
            SwNode* pFirstTextNode = &aIdx.GetNode();
            SwNodeOffset nEnd = pStartFly->EndOfSectionIndex();
            while (aIdx < nEnd)
            {
                if (pFirstTextNode->IsContentNode() && pFirstTextNode->IsTextNode())
                {
                    if (aIdx == aCurrentIdx)
                    {
                        auto pIssue = lclAddIssue(m_rIssueCollection, SwResId(STR_FLOATING_TEXT),
                                                  sfx::AccessibilityIssueID::FLOATING_TEXT,
                                                  sfx::AccessibilityIssueLevel::WARNLEV);
                        pIssue->setIssueObject(IssueObject::TEXTFRAME);
                        pIssue->setObjectID(pFormat->GetName().toString());
                        pIssue->setDoc(pCurrent->GetDoc());
                        pIssue->setNode(pCurrent);
                    }
                    break;
                }
                ++aIdx;
                pFirstTextNode = &aIdx.GetNode();
            }
        }
    }
};

/// Heading paragraphs (with outline levels > 0) are not allowed in tables
class TableHeadingCheck : public NodeCheck
{
private:
    // Boolean indicating if heading-in-table warning is already triggered.
    bool m_bPrevPassed;

public:
    TableHeadingCheck(sfx::AccessibilityIssueCollection& rIssueCollection)
        : NodeCheck(rIssueCollection)
        , m_bPrevPassed(true)
    {
    }

    void check(SwNode* pCurrent) override
    {
        if (!m_bPrevPassed)
            return;

        const SwTextNode* textNode = pCurrent->GetTextNode();

        if (textNode && textNode->GetAttrOutlineLevel() != 0)
        {
            const SwTableNode* parentTable = pCurrent->FindTableNode();

            if (parentTable)
            {
                m_bPrevPassed = false;
                auto pIssue = lclAddIssue(m_rIssueCollection, SwResId(STR_HEADING_IN_TABLE),
                                          sfx::AccessibilityIssueID::HEADING_IN_TABLE,
                                          sfx::AccessibilityIssueLevel::WARNLEV);
                pIssue->setIssueObject(IssueObject::TEXT);
                pIssue->setDoc(pCurrent->GetDoc());
                pIssue->setNode(pCurrent);
            }
        }
    }
};

/// Checking if headings are ordered correctly.
class HeadingOrderCheck : public NodeCheck
{
public:
    HeadingOrderCheck(sfx::AccessibilityIssueCollection& rIssueCollection)
        : NodeCheck(rIssueCollection)
    {
    }

    void check(SwNode* pCurrent) override
    {
        const SwTextNode* pTextNode = pCurrent->GetTextNode();
        if (!pTextNode)
            return;

        // If outline level stands for heading level...
        const int currentLevel = pTextNode->GetAttrOutlineLevel();
        if (!currentLevel)
            return;

        // ... and if is bigger than previous by more than 1, warn.
        if (currentLevel - m_prevLevel > 1)
        {
            // Preparing and posting a warning.
            OUString resultString;
            sfx::AccessibilityIssueID eIssueID;
            if (!m_prevLevel)
            {
                resultString = SwResId(STR_HEADING_START);
                eIssueID = sfx::AccessibilityIssueID::HEADING_START;
            }
            else
            {
                resultString = SwResId(STR_HEADING_ORDER);
                resultString
                    = resultString.replaceAll("%LEVEL_PREV%", OUString::number(m_prevLevel));
                eIssueID = sfx::AccessibilityIssueID::HEADING_ORDER;
            }
            resultString
                = resultString.replaceAll("%LEVEL_CURRENT%", OUString::number(currentLevel));
            auto pIssue = lclAddIssue(m_rIssueCollection, resultString, eIssueID,
                                      sfx::AccessibilityIssueLevel::ERRORLEV);
            pIssue->setIssueObject(IssueObject::TEXT);
            pIssue->setDoc(pCurrent->GetDoc());
            pIssue->setNode(pCurrent);
        }

        // Updating previous level.
        m_prevLevel = currentLevel;
    }

private:
    // Previous heading level to compare with.
    int m_prevLevel = 0;
};

/// Checking content controls in header or footer
class ContentControlCheck : public NodeCheck
{
public:
    ContentControlCheck(sfx::AccessibilityIssueCollection& rIssueCollection)
        : NodeCheck(rIssueCollection)
    {
    }

    void check(SwNode* pCurrent) override
    {
        if (!pCurrent->IsContentNode())
            return;

        const SwTextNode* pTextNode = pCurrent->GetTextNode();
        if (pTextNode)
        {
            if (pCurrent->FindHeaderStartNode() || pCurrent->FindFooterStartNode())
            {
                const SwpHints* pHts = pTextNode->GetpSwpHints();
                if (pHts)
                {
                    for (size_t i = 0; i < pHts->Count(); ++i)
                    {
                        const SwTextAttr* pHt = pHts->Get(i);
                        if (pHt->Which() == RES_TXTATR_CONTENTCONTROL)
                        {
                            auto pIssue
                                = lclAddIssue(m_rIssueCollection,
                                              SwResId(STR_CONTENT_CONTROL_IN_HEADER_OR_FOOTER),
                                              sfx::AccessibilityIssueID::CONTENT_CONTROL,
                                              sfx::AccessibilityIssueLevel::WARNLEV);
                            pIssue->setIssueObject(IssueObject::TEXT);
                            pIssue->setDoc(pCurrent->GetDoc());
                            pIssue->setNode(pCurrent);
                            break;
                        }
                    }
                }
            }
        }
    }
};

class EmptyLineBetweenNumberingCheck : public NodeCheck
{
private:
    static SwTextNode* getPrevTextNode(SwNode* pCurrent)
    {
        SwTextNode* pTextNode = nullptr;

        auto nIndex = pCurrent->GetIndex();

        nIndex--; // go to previous node

        while (pTextNode == nullptr && nIndex >= SwNodeOffset(0))
        {
            auto pNode = pCurrent->GetNodes()[nIndex];
            if (pNode->IsTextNode())
                pTextNode = pNode->GetTextNode();
            nIndex--;
        }

        return pTextNode;
    }

    static SwTextNode* getNextTextNode(SwNode* pCurrent)
    {
        SwTextNode* pTextNode = nullptr;

        auto nIndex = pCurrent->GetIndex();

        nIndex++; // go to next node

        while (pTextNode == nullptr && nIndex < pCurrent->GetNodes().Count())
        {
            auto pNode = pCurrent->GetNodes()[nIndex];
            if (pNode->IsTextNode())
                pTextNode = pNode->GetTextNode();
            nIndex++;
        }

        return pTextNode;
    }

public:
    EmptyLineBetweenNumberingCheck(sfx::AccessibilityIssueCollection& rIssueCollection)
        : NodeCheck(rIssueCollection)
    {
    }
    void check(SwNode* pCurrent) override
    {
        if (!pCurrent->IsTextNode())
            return;

        // Don't count empty table box text nodes
        if (pCurrent->GetTableBox())
            return;

        SwTextNode* pTextNode = pCurrent->GetTextNode();
        SwDoc& rDocument = pTextNode->GetDoc();
        SwDocShell* pDocShell = rDocument.GetDocShell();
        if (!pDocShell)
            return;

        SwWrtShell* pWrtShell = pDocShell->GetWrtShell();
        if (!pWrtShell)
            return;

        auto nParagraphLength = pTextNode->GetText().getLength();
        if (nParagraphLength == 0 && !pTextNode->GetNumRule())
        {
            SwTextNode* pPrevTextNode = getPrevTextNode(pCurrent);
            if (!pPrevTextNode)
                return;

            SwTextNode* pNextTextNode = getNextTextNode(pCurrent);
            if (!pNextTextNode)
                return;

            if (pPrevTextNode->getLayoutFrame(pWrtShell->GetLayout())
                && pNextTextNode->getLayoutFrame(pWrtShell->GetLayout()))
            {
                const SwNumRule* pPrevRule = pPrevTextNode->GetNumRule();
                const SwNumRule* pNextRule = pNextTextNode->GetNumRule();
                if (pPrevRule && pNextRule)
                {
                    auto pIssue = lclAddIssue(m_rIssueCollection, SwResId(STR_AVOID_EMPTY_NUM_PARA),
                                              sfx::AccessibilityIssueID::TEXT_EMPTY_NUM_PARA,
                                              sfx::AccessibilityIssueLevel::WARNLEV);
                    pIssue->setIssueObject(IssueObject::TEXT);
                    pIssue->setNode(pTextNode);
                    pIssue->setDoc(rDocument);
                }
            }
        }
    }
};

class DocumentCheck : public BaseCheck
{
public:
    DocumentCheck(sfx::AccessibilityIssueCollection& rIssueCollection)
        : BaseCheck(rIssueCollection)
    {
    }

    virtual void check(SwDoc* pDoc) = 0;
};

// Check default language
class DocumentDefaultLanguageCheck : public DocumentCheck
{
public:
    DocumentDefaultLanguageCheck(sfx::AccessibilityIssueCollection& rIssueCollection)
        : DocumentCheck(rIssueCollection)
    {
    }

    void check(SwDoc* pDoc) override
    {
        // TODO maybe - also check RES_CHRATR_CJK_LANGUAGE, RES_CHRATR_CTL_LANGUAGE if CJK or CTL are enabled
        const SvxLanguageItem& rLang = pDoc->GetDefault(RES_CHRATR_LANGUAGE);
        LanguageType eLanguage = rLang.GetLanguage();
        if (eLanguage == LANGUAGE_NONE)
        {
            auto pIssue = lclAddIssue(m_rIssueCollection, SwResId(STR_DOCUMENT_DEFAULT_LANGUAGE),
                                      sfx::AccessibilityIssueID::DOCUMENT_LANGUAGE,
                                      sfx::AccessibilityIssueLevel::WARNLEV);
            pIssue->setIssueObject(IssueObject::LANGUAGE_NOT_SET);
            pIssue->setObjectID(OUString());
            pIssue->setDoc(*pDoc);
        }
        else
        {
            for (SwTextFormatColl* pTextFormatCollection : *pDoc->GetTextFormatColls())
            {
                const SwAttrSet& rAttrSet = pTextFormatCollection->GetAttrSet();
                if (rAttrSet.GetLanguage(false).GetLanguage() == LANGUAGE_NONE)
                {
                    UIName sName = pTextFormatCollection->GetName();
                    OUString sIssueText = SwResId(STR_STYLE_NO_LANGUAGE)
                                              .replaceAll("%STYLE_NAME%", sName.toString());

                    auto pIssue = lclAddIssue(m_rIssueCollection, sIssueText,
                                              sfx::AccessibilityIssueID::STYLE_LANGUAGE,
                                              sfx::AccessibilityIssueLevel::WARNLEV);
                    pIssue->setIssueObject(IssueObject::LANGUAGE_NOT_SET);
                    pIssue->setObjectID(sName.toString());
                    pIssue->setDoc(*pDoc);
                }
            }
        }
    }
};

class DocumentTitleCheck : public DocumentCheck
{
public:
    DocumentTitleCheck(sfx::AccessibilityIssueCollection& rIssueCollection)
        : DocumentCheck(rIssueCollection)
    {
    }

    void check(SwDoc* pDoc) override
    {
        SwDocShell* pShell = pDoc->GetDocShell();
        if (!pShell)
            return;

        const uno::Reference<document::XDocumentPropertiesSupplier> xDPS(pShell->GetModel(),
                                                                         uno::UNO_QUERY_THROW);
        const uno::Reference<document::XDocumentProperties> xDocumentProperties(
            xDPS->getDocumentProperties());
        OUString sTitle = xDocumentProperties->getTitle();
        if (o3tl::trim(sTitle).empty())
        {
            auto pIssue = lclAddIssue(m_rIssueCollection, SwResId(STR_DOCUMENT_TITLE),
                                      sfx::AccessibilityIssueID::DOCUMENT_TITLE,
                                      sfx::AccessibilityIssueLevel::ERRORLEV);
            pIssue->setDoc(*pDoc);
            pIssue->setIssueObject(IssueObject::DOCUMENT_TITLE);
        }
    }
};

class FootnoteEndnoteCheck : public DocumentCheck
{
public:
    FootnoteEndnoteCheck(sfx::AccessibilityIssueCollection& rIssueCollection)
        : DocumentCheck(rIssueCollection)
    {
    }

    void check(SwDoc* pDoc) override
    {
        for (SwTextFootnote* pTextFootnote : pDoc->GetFootnoteIdxs())
        {
            SwFormatFootnote const& rFootnote = pTextFootnote->GetFootnote();
            OUString sError = rFootnote.IsEndNote() ? SwResId(STR_AVOID_ENDNOTES)
                                                    : SwResId(STR_AVOID_FOOTNOTES);
            sfx::AccessibilityIssueID eIssueID = rFootnote.IsEndNote()
                                                     ? sfx::AccessibilityIssueID::AVOID_FOOTNOTES
                                                     : sfx::AccessibilityIssueID::AVOID_ENDNOTES;
            auto pIssue = lclAddIssue(m_rIssueCollection, sError, eIssueID,
                                      sfx::AccessibilityIssueLevel::WARNLEV);
            pIssue->setDoc(*pDoc);
            pIssue->setIssueObject(IssueObject::FOOTENDNOTE);
            pIssue->setTextFootnote(pTextFootnote);
        }
    }
};

class BackgroundImageCheck : public DocumentCheck
{
public:
    BackgroundImageCheck(sfx::AccessibilityIssueCollection& rIssueCollection)
        : DocumentCheck(rIssueCollection)
    {
    }
    void check(SwDoc* pDoc) override
    {
        SwDocShell* pDocShell = pDoc->GetDocShell();
        if (!pDocShell)
            return;
        rtl::Reference<SwXTextDocument> xDoc = pDocShell->GetBaseModel();
        if (!xDoc)
            return;
        uno::Reference<container::XNameAccess> xStyleFamilies = xDoc->getStyleFamilies();
        uno::Reference<container::XNameAccess> xStyleFamily(
            xStyleFamilies->getByName(u"PageStyles"_ustr), uno::UNO_QUERY);
        if (!xStyleFamily.is())
            return;
        const uno::Sequence<OUString> xStyleFamilyNames = xStyleFamily->getElementNames();
        for (const OUString& rStyleFamilyName : xStyleFamilyNames)
        {
            uno::Reference<beans::XPropertySet> xPropertySet(
                xStyleFamily->getByName(rStyleFamilyName), uno::UNO_QUERY);
            if (!xPropertySet.is())
                continue;
            auto aFillStyleContainer = xPropertySet->getPropertyValue(u"FillStyle"_ustr);
            if (aFillStyleContainer.has<drawing::FillStyle>())
            {
                drawing::FillStyle aFillStyle = aFillStyleContainer.get<drawing::FillStyle>();
                if (aFillStyle == drawing::FillStyle_BITMAP)
                {
                    auto pIssue
                        = lclAddIssue(m_rIssueCollection, SwResId(STR_AVOID_BACKGROUND_IMAGES),
                                      sfx::AccessibilityIssueID::DOCUMENT_BACKGROUND,
                                      sfx::AccessibilityIssueLevel::WARNLEV);

                    pIssue->setDoc(*pDoc);
                    pIssue->setIssueObject(IssueObject::DOCUMENT_BACKGROUND);
                }
            }
        }
    }
};

} // end anonymous namespace

// Check Shapes, TextBox
void AccessibilityCheck::checkObject(SwNode* pCurrent, SwFrameFormat const& rFrameFormat)
{
    SdrObject const* const pObject{ rFrameFormat.FindSdrObject() };
    if (!pObject)
        return;

    // check hyperlink
    if (SwFormatURL const* const pItem{ rFrameFormat.GetItemIfSet(RES_URL, false) })
    {
        OUString const sHyperlink{ pItem->GetURL() };
        if (!sHyperlink.isEmpty() && pItem->GetName().isEmpty())
        {
            INetURLObject const aHyperlink(sHyperlink);
            if (aHyperlink.GetProtocol() != INetProtocol::NotValid)
            {
                std::shared_ptr<sw::AccessibilityIssue> pNameIssue
                    = lclAddIssue(m_aIssueCollection, SwResId(STR_HYPERLINK_NO_NAME),
                                  sfx::AccessibilityIssueID::HYPERLINK_NO_NAME,
                                  sfx::AccessibilityIssueLevel::WARNLEV);

                if (pNameIssue)
                {
                    pNameIssue->setIssueObject(IssueObject::HYPERLINKFLY);
                    pNameIssue->setObjectID(rFrameFormat.GetName().toString());
                    pNameIssue->setNode(pCurrent);
                    pNameIssue->setDoc(*m_pDoc);
                }
            }
        }
    }

    // Check for fontworks.
    if (SdrObjCustomShape const* pCustomShape = dynamic_cast<SdrObjCustomShape const*>(pObject))
    {
        const SdrCustomShapeGeometryItem& rGeometryItem
            = pCustomShape->GetMergedItem(SDRATTR_CUSTOMSHAPE_GEOMETRY);

        if (const uno::Any* pAny = rGeometryItem.GetPropertyValueByName(u"Type"_ustr))
            if (pAny->get<OUString>().startsWith("fontwork-"))
                lclAddIssue(m_aIssueCollection, SwResId(STR_FONTWORKS),
                            sfx::AccessibilityIssueID::FONTWORKS,
                            sfx::AccessibilityIssueLevel::WARNLEV);
    }

    // Checking if there is floating Writer text draw object and if so, throwing a warning.
    // (Floating objects with text create problems with reading order)
    if (pObject->HasText()
        && FindFrameFormat(pObject)->GetAnchor().GetAnchorId() != RndStdIds::FLY_AS_CHAR)
    {
        auto pIssue = lclAddIssue(m_aIssueCollection, SwResId(STR_FLOATING_TEXT),
                                  sfx::AccessibilityIssueID::FLOATING_TEXT,
                                  sfx::AccessibilityIssueLevel::WARNLEV);
        pIssue->setIssueObject(IssueObject::TEXTFRAME);
        pIssue->setObjectID(pObject->GetName());
        pIssue->setDoc(*m_pDoc);
        if (pCurrent)
            pIssue->setNode(pCurrent);
    }

    // Graphic, OLE for alt (title) text already checked in NoTextNodeAltTextCheck
    if (pObject->GetObjIdentifier() != SdrObjKind::SwFlyDrawObjIdentifier)
    {
        if (!pObject->IsDecorative() && pObject->GetTitle().isEmpty()
            && pObject->GetDescription().isEmpty())
        {
            const OUString& sName = pObject->GetName();
            OUString sIssueText = SwResId(STR_NO_ALT).replaceAll("%OBJECT_NAME%", sName);
            auto pIssue = lclAddIssue(m_aIssueCollection, sIssueText,
                                      sfx::AccessibilityIssueID::NO_ALT_SHAPE,
                                      sfx::AccessibilityIssueLevel::ERRORLEV);
            // Set FORM Issue for Form objects because of the design mode
            if (pObject->GetObjInventor() == SdrInventor::FmForm)
                pIssue->setIssueObject(IssueObject::FORM);
            else
                pIssue->setIssueObject(IssueObject::SHAPE);

            pIssue->setObjectID(pObject->GetName());
            pIssue->setDoc(*m_pDoc);
            if (pCurrent)
                pIssue->setNode(pCurrent);
        }
    }
}

void AccessibilityCheck::init()
{
    if (m_aDocumentChecks.empty())
    {
        m_aDocumentChecks.emplace_back(new DocumentDefaultLanguageCheck(m_aIssueCollection));
        m_aDocumentChecks.emplace_back(new DocumentTitleCheck(m_aIssueCollection));
        m_aDocumentChecks.emplace_back(new FootnoteEndnoteCheck(m_aIssueCollection));
        m_aDocumentChecks.emplace_back(new BackgroundImageCheck(m_aIssueCollection));
    }

    if (m_aNodeChecks.empty())
    {
        m_aNodeChecks.emplace_back(new NoTextNodeAltTextCheck(m_aIssueCollection));
        m_aNodeChecks.emplace_back(new TableNodeMergeSplitCheck(m_aIssueCollection));
        m_aNodeChecks.emplace_back(new TableFormattingCheck(m_aIssueCollection));
        m_aNodeChecks.emplace_back(new NumberingCheck(m_aIssueCollection));
        m_aNodeChecks.emplace_back(new HyperlinkCheck(m_aIssueCollection));
        m_aNodeChecks.emplace_back(new TextContrastCheck(m_aIssueCollection));
        m_aNodeChecks.emplace_back(new BlinkingTextCheck(m_aIssueCollection));
        m_aNodeChecks.emplace_back(new HeaderCheck(m_aIssueCollection));
        m_aNodeChecks.emplace_back(new TextFormattingCheck(m_aIssueCollection));
        m_aNodeChecks.emplace_back(new NonInteractiveFormCheck(m_aIssueCollection));
        m_aNodeChecks.emplace_back(new FloatingTextCheck(m_aIssueCollection));
        m_aNodeChecks.emplace_back(new TableHeadingCheck(m_aIssueCollection));
        m_aNodeChecks.emplace_back(new HeadingOrderCheck(m_aIssueCollection));
        m_aNodeChecks.emplace_back(new NewlineSpacingCheck(m_aIssueCollection));
        m_aNodeChecks.emplace_back(new SpaceSpacingCheck(m_aIssueCollection));
        m_aNodeChecks.emplace_back(new FakeFootnoteCheck(m_aIssueCollection));
        m_aNodeChecks.emplace_back(new FakeCaptionCheck(m_aIssueCollection));
        m_aNodeChecks.emplace_back(new ContentControlCheck(m_aIssueCollection));
        m_aNodeChecks.emplace_back(new EmptyLineBetweenNumberingCheck(m_aIssueCollection));
    }
}

void AccessibilityCheck::checkNode(SwNode* pNode)
{
    if (m_pDoc == nullptr || pNode == nullptr)
        return;

    init();

    for (std::shared_ptr<BaseCheck>& rpNodeCheck : m_aNodeChecks)
    {
        auto pNodeCheck = dynamic_cast<NodeCheck*>(rpNodeCheck.get());
        if (pNodeCheck)
            pNodeCheck->check(pNode);
    }
}

void AccessibilityCheck::checkDocumentProperties()
{
    if (m_pDoc == nullptr)
        return;

    init();

    for (std::shared_ptr<BaseCheck>& rpDocumentCheck : m_aDocumentChecks)
    {
        auto pDocumentCheck = dynamic_cast<DocumentCheck*>(rpDocumentCheck.get());
        if (pDocumentCheck)
            pDocumentCheck->check(m_pDoc);
    }
}

void AccessibilityCheck::check()
{
    if (m_pDoc == nullptr)
        return;

    init();

    checkDocumentProperties();

    auto const& pNodes = m_pDoc->GetNodes();
    SwNode* pNode = nullptr;
    for (SwNodeOffset n(0); n < pNodes.Count(); ++n)
    {
        pNode = pNodes[n];
        if (pNode)
        {
            for (std::shared_ptr<BaseCheck>& rpNodeCheck : m_aNodeChecks)
            {
                auto pNodeCheck = dynamic_cast<NodeCheck*>(rpNodeCheck.get());
                if (pNodeCheck)
                    pNodeCheck->check(pNode);
            }

            for (SwFrameFormat* const& pFrameFormat : pNode->GetAnchoredFlys())
            {
                checkObject(pNode, *pFrameFormat);
            }
        }
    }
}

} // end sw namespace

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
