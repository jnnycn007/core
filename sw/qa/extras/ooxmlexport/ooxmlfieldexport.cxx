/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <swmodeltestbase.hxx>

#include <com/sun/star/text/XDocumentIndex.hpp>
#include <com/sun/star/text/XTextFieldsSupplier.hpp>
#include <com/sun/star/text/XTextField.hpp>
#include <com/sun/star/text/XTextTable.hpp>

#include <xmloff/odffields.hxx>
#include <o3tl/string_view.hxx>

#include <IDocumentMarkAccess.hxx>
#include <IMark.hxx>
#include <docsh.hxx>
#include <unotxdoc.hxx>

class Test : public SwModelTestBase
{
public:
    Test() : SwModelTestBase(u"/sw/qa/extras/ooxmlexport/data/"_ustr, u"Office Open XML Text"_ustr) {}
};

DECLARE_OOXMLEXPORT_TEST(testFdo47669, "fdo47669.docx")
{
    /*
     * Problem: we created imbalance </w:hyperlink> which shouldn't be there,
     * resulting in loading error: missing last character of hyperlink text
     * and content after it wasn't loaded.
     */
    getParagraph(1, u"This is a hyperlink with anchor. Also, this sentence should be seen."_ustr);
    getRun(getParagraph(1), 2, u"hyperlink with anchor"_ustr);
    CPPUNIT_ASSERT_EQUAL(u"http://www.google.com/#a"_ustr, getProperty<OUString>(getRun(getParagraph(1), 2), u"HyperLinkURL"_ustr));
}

DECLARE_OOXMLEXPORT_TEST(testN789482, "n789482.docx")
{
    // The problem was that w:del was exported before w:hyperlink, resulting in an invalid XML.
    uno::Reference<text::XTextRange> xParagraph = getParagraph(1);
    getRun(xParagraph, 1, u"Before. "_ustr);

    CPPUNIT_ASSERT_EQUAL(u"Delete"_ustr, getProperty<OUString>(getRun(xParagraph, 2), u"RedlineType"_ustr));
    CPPUNIT_ASSERT_EQUAL(true, getProperty<bool>(getRun(xParagraph, 2), u"IsStart"_ustr));

    getRun(xParagraph, 3, u"www.test.com"_ustr);
    CPPUNIT_ASSERT_EQUAL(u"http://www.test.com/"_ustr, getProperty<OUString>(getRun(xParagraph, 3), u"HyperLinkURL"_ustr));

    CPPUNIT_ASSERT_EQUAL(u"Delete"_ustr, getProperty<OUString>(getRun(xParagraph, 4), u"RedlineType"_ustr));
    CPPUNIT_ASSERT_EQUAL(false, getProperty<bool>(getRun(xParagraph, 4), u"IsStart"_ustr));

    getRun(xParagraph, 5, u" After."_ustr);
}

CPPUNIT_TEST_FIXTURE(Test, testBnc834035)
{
    loadAndReload("bnc834035.odt");
    CPPUNIT_ASSERT_EQUAL(2, getShapes());
    CPPUNIT_ASSERT_EQUAL(3, getPages());
    // Illustration index had wrong hyperlinks: anchor was using Writer's
    // <seqname>!<index>|sequence syntax, not a bookmark name.
    xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);
    // This was Figure!1|sequence.
    assertXPath(pXmlDoc, "/w:document/w:body/w:p[10]/w:hyperlink", "anchor", u"_Toc363553908");
}

CPPUNIT_TEST_FIXTURE(Test, testCp1000015)
{
    loadAndReload("cp1000015.odt");
    CPPUNIT_ASSERT_EQUAL(1, getPages());
    // Redline and hyperlink end got exported in an incorrect order.
    getParagraph(1, u"Hello."_ustr);
    getParagraph(2, u"http://www.google.com/"_ustr);
}

CPPUNIT_TEST_FIXTURE(Test, testHyperlineIsEnd)
{
    loadAndReload("hyperlink.docx");
    // Check  that the document.xml contents all the tag properly closed.
    xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);
    // If  document.xml miss any ending tag then parseExport() returns NULL which fail the test case.
    CPPUNIT_ASSERT(pXmlDoc) ;
    // Check hyperlink is properly open.
    assertXPath(pXmlDoc, "/w:document/w:body/w:sdt/w:sdtContent/w:p/w:hyperlink",1);
}

CPPUNIT_TEST_FIXTURE(Test, testFdo69649)
{
    loadAndReload("fdo69649.docx");
    // The DOCX containing the Table of Contents was not exported with correct page nos
    xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);

    assertXPathContent(pXmlDoc, "/w:document/w:body/w:sdt/w:sdtContent/w:p[21]/w:hyperlink/w:r[5]/w:t", u"15");
}

CPPUNIT_TEST_FIXTURE(Test, testFieldFlagO)
{
    loadAndReload("TOC_field_f.docx");
    // This test case is to verify \o flag should come once.
    xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);

    // FIXME "p[2]" will have to be "p[1]", once the TOC import code is fixed
    // not to insert an empty paragraph before TOC.
    assertXPathContent(pXmlDoc, "/w:document/w:body/w:sdt/w:sdtContent/w:p[1]/w:r[2]/w:instrText", u" TOC \\z \\f \\o \"1-3\" \\u \\h");
}

CPPUNIT_TEST_FIXTURE(Test, testTOCFlag_f)
{
    loadAndReload("toc_doc.docx");
    // Export logic for all TOC field flags was enclosed inside
    // if( SwTOXElement::Mark & pTOX->GetCreateType() ) in ww8atr.cxx which gets true for \f,
    // this was the reason if there is \f flag present in original doc then only other flags like
    // \o \h \n used to come after RoundTrip.
    // This test case is to verify even if there is no \f flag in original doc, \h flag is getting
    // preserved after RT.
    xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);

    // FIXME "p[2]" will have to be "p[1]", once the TOC import code is fixed
    // not to insert an empty paragraph before TOC.
    assertXPathContent(pXmlDoc, "/w:document/w:body/w:sdt/w:sdtContent/w:p[1]/w:r[2]/w:instrText", u" TOC \\z \\o \"1-3\" \\u \\h");
}

CPPUNIT_TEST_FIXTURE(Test, testPreserveZfield)
{
    loadAndReload("preserve_Z_field_TOC.docx");
    xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);

    assertXPathContent(pXmlDoc, "/w:document/w:body/w:sdt/w:sdtContent/w:p[1]/w:r[2]/w:instrText", u" TOC \\z \\f \\o \"1-3\" \\h");
}

CPPUNIT_TEST_FIXTURE(Test, testPreserveWfieldTOC)
{
    loadAndReload("PreserveWfieldTOC.docx");
    xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);

    assertXPathContent(pXmlDoc, "/w:document/w:body/w:sdt/w:sdtContent/w:p/w:r[2]/w:instrText", u" TOC \\z \\w \\f \\o \"1-3\" \\h");
}

CPPUNIT_TEST_FIXTURE(Test, testFieldFlagB)
{
    loadAndReload("TOC_field_b.docx");
    // This test case is to verify \b flag.
    xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);

    // FIXME "p[2]" will have to be "p[1]", once the TOC import code is fixed
    // not to insert an empty paragraph before TOC.
    assertXPathContent(pXmlDoc, "/w:document/w:body/w:sdt/w:sdtContent/w:p[1]/w:r[2]/w:instrText", u" TOC \\b \"bookmark111\" \\o \"1-9\" \\h");
}

CPPUNIT_TEST_FIXTURE(Test, testPreserveXfieldTOC)
{
    loadAndReload("PreserveXfieldTOC.docx");
    xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);

    assertXPathContent(pXmlDoc, "/w:document/w:body/w:sdt/w:sdtContent/w:p/w:r[2]/w:instrText", u" TOC \\x \\f \\o \"1-3\" \\h");
}

CPPUNIT_TEST_FIXTURE(Test, testFDO77715)
{
    loadAndReload("FDO77715.docx");
    xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);

    // tdf#153090 check that para style is preserved
    assertXPathContent(pXmlDoc, "/w:document/w:body/w:p[3]/w:r[2]/w:instrText[1]", u" TOC \\c \\h \\t \"Block Header\" ");

    uno::Reference<text::XDocumentIndexesSupplier> xIndexSupplier(mxComponent, uno::UNO_QUERY);
    uno::Reference<container::XIndexAccess> xIndexes = xIndexSupplier->getDocumentIndexes();
    uno::Reference<text::XDocumentIndex> xTOC(xIndexes->getByIndex(0), uno::UNO_QUERY);
    CPPUNIT_ASSERT_EQUAL(u"Block Header"_ustr, getProperty<OUString>(xTOC, u"CreateFromParagraphStyle"_ustr));
    // tdf#153090 check that update uses the style
    xTOC->update();
    OUString const tocContent(xTOC->getAnchor()->getString());
    CPPUNIT_ASSERT(tocContent.startsWith("National Infrastructure Bank Aff/Neg Index"));
    // check that 1st paragraph has outline level 1
    CPPUNIT_ASSERT_EQUAL(sal_Int32(1), getProperty<sal_Int32>(getParagraph(1), u"OutlineLevel"_ustr));
}

CPPUNIT_TEST_FIXTURE(Test, testTOCFlag_u)
{
    loadAndReload("testTOCFlag_u.docx");
    // DOCX containing TOC should preserve code field '\u'.
    xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);

    // FIXME "p[2]" will have to be "p[1]", once the TOC import code is fixed
    // not to insert an empty paragraph before TOC.
    assertXPathContent(pXmlDoc, "/w:document/w:body/w:sdt/w:sdtContent/w:p/w:r[2]/w:instrText", u" TOC \\z \\o \"1-9\" \\u \\h");
}

CPPUNIT_TEST_FIXTURE(Test, testfdo73596_RunInStyle)
{
    loadAndReload("fdo73596_RunInStyle.docx");
    // INDEX should be preserved.
    xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);

    assertXPathContent(pXmlDoc, "/w:document/w:body/w:p[2]/w:r[2]/w:instrText[1]", u" INDEX \\e \"");
}

CPPUNIT_TEST_FIXTURE(Test, testfdo73596_AlphaSeparator)
{
    loadAndReload("fdo73596_AlphaSeparator.docx");
    // INDEX flag \h "A" should be preserved.
    xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);

    assertXPathContent(pXmlDoc, "/w:document/w:body/w:p[2]/w:r[2]/w:instrText[1]", u" INDEX \\h \"A\" \\e \"");
}

CPPUNIT_TEST_FIXTURE(Test, testCaption1)
{
    loadAndReload("EquationAsScientificNumbering.docx");
    // fdo#74431 : This test case is to verify the Captions are coming properly
    // earlier it was coming as "SEQ "scientific"\*ROMAN now it is SEQ scientific\* ROMAN"

    xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);

    assertXPathContent(pXmlDoc, "/w:document/w:body/w:p/w:r[3]/w:instrText", u" SEQ scientific \\* ROMAN ");
}

CPPUNIT_TEST_FIXTURE(Test, testCaption2)
{
    loadAndReload("EquationWithAboveAndBelowCaption.docx");
    // fdo#72563 : There was a problem that in case of TOC,PAGEREF field tag was not preserved during Roundtrip
    // This test case is to verify that PAGEREF tag is coming with proper values inside <hyperlink> tag.
    xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);

    assertXPathContent(pXmlDoc, "/w:document/w:body/w:p[5]/w:r[3]/w:instrText", u" SEQ Equation \\* ARABIC ");
}

CPPUNIT_TEST_FIXTURE(Test, testCaption3)
{
    loadAndReload("FigureAsLabelPicture.docx");
    // fdo#72563 : There was a problem that in case of TOC,PAGEREF field tag was not preserved during Roundtrip
    // This test case is to verify that PAGEREF tag is coming with proper values inside <hyperlink> tag.
    xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);

    assertXPathContent(pXmlDoc, "/w:document/w:body/w:p[2]/w:r[3]/w:instrText", u" SEQ picture \\* ARABIC ");
}

CPPUNIT_TEST_FIXTURE(Test, testCaption4)
{
    loadAndReload("TableWithAboveCaptions.docx");
    // fdo#72563 : There was a problem that in case of TOC,PAGEREF field tag was not preserved during Roundtrip
    // This test case is to verify that PAGEREF tag is coming with proper values inside <hyperlink> tag.
    xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);

    assertXPathContent(pXmlDoc, "/w:document/w:body/w:p[1]/w:r[3]/w:instrText", u" SEQ Table \\* ARABIC ");
}

CPPUNIT_TEST_FIXTURE(Test, testFooterContainHyperlink)
{
    loadAndReload("footer-contain-hyperlink.docx");
    // Problem is that footer1.xml.rels contains the empty
    // Target due to which the file get corrupted
    // in MS Office 2007.
    // Check for footer1.xml.rels file.
    xmlDocUniquePtr pXmlRels = parseExport(u"word/_rels/footer2.xml.rels"_ustr);
    // Check the value of Target which is http://www.google.com/.
    assertXPath(pXmlRels,"/rels:Relationships/rels:Relationship","Target",u"http://www.google.com/");
}

CPPUNIT_TEST_FIXTURE(Test, testAlphabeticalIndex_MultipleColumns)
{
    loadAndReload("alphabeticalIndex_MultipleColumns.docx");
    // Bug :: fdo#73596
    /*
     * Index with multiple columns was not imported correctly and
     * hence not exported correctly...
     * The column count is given by the \c switch.
     * If the column count is explicitly specified,
     * MS Office adds section breaks before and after the Index.
     */
    xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);

    assertXPathContent(pXmlDoc, "/w:document/w:body/w:p[3]/w:r[2]/w:instrText", u" INDEX \\c \"4\"\\e \"");

    // check for section breaks after and before the Index Section
    assertXPath(pXmlDoc, "/w:document/w:body/w:p[2]/w:pPr/w:sectPr/w:type","val",u"continuous");
    assertXPath(pXmlDoc, "/w:document/w:body/w:p[8]/w:pPr/w:sectPr/w:type","val",u"continuous");
    // check for "w:space" attribute for the columns in Section Properties
    assertXPath(pXmlDoc, "/w:document/w:body/w:p[8]/w:pPr/w:sectPr/w:cols","space",u"720");
}

CPPUNIT_TEST_FIXTURE(Test, testPageref)
{
    loadAndReload("testPageref.docx");
    // fdo#72563 : There was a problem that in case of TOC,PAGEREF field tag was not preserved during Roundtrip
    // This test case is to verify that PAGEREF tag is coming with proper values inside <hyperlink> tag.
    xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);

    assertXPathContent(pXmlDoc, "/w:document/w:body/w:sdt/w:sdtContent/w:p[2]/w:hyperlink/w:r[3]/w:instrText", u"PAGEREF _Toc355095261 \\h");
}

CPPUNIT_TEST_FIXTURE(Test, testAlphabeticalIndex_AutoColumn)
{
    loadAndReload("alphabeticalIndex_AutoColumn.docx");
    // Bug :: fdo#73596
    /*
     * When the columns in Index are 0; i.e not specified by the
     * "\c" switch, don't write back '\c "0"' or the section breaks
     * before and after the Index Context
     */
    xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);

    assertXPathContent(pXmlDoc, "/w:document/w:body/w:p[2]/w:r[2]/w:instrText", u" INDEX \\e \"");

    // check for section break doesn't appear for any paragraph
    assertXPath(pXmlDoc, "/w:document/w:body/w:p/w:pPr/w:sectPr", 0);
}

CPPUNIT_TEST_FIXTURE(Test, testIndexFieldFlagF)
{
    loadAndReload("IndexFieldFlagF.docx");
    // This test case is to verify the Index field flag '\f' with some
    // Specific Entry Type (ex. "Syn" in our case).
    xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);
    // We check the Index field flag '\f'.
    // Note: no syntax error any more (extra quotation mark between "Syn" and \e)
    assertXPathContent(pXmlDoc, "/w:document[1]/w:body[1]/w:p[4]/w:r[2]/w:instrText[1]", u" INDEX \\c \"2\"\\f \"Syn\"\\e \"");
    // XE entries lost their \f "Syn" before
    assertXPathContent(pXmlDoc, "/w:document[1]/w:body[1]/w:p[17]/w:r[21]/w:instrText[1]", u" XE \"formatting\" \\f \"Syn\" ");
    assertXPathContent(pXmlDoc, "/w:document[1]/w:body[1]/w:p[17]/w:r[29]/w:instrText[1]", u" XE \"choosing:aaaa\" \\f \"Syn\" ");
}

CPPUNIT_TEST_FIXTURE(Test, testBibliography)
{
    loadAndReload("FDO75133.docx");
    xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);

    assertXPathContent(pXmlDoc, "/w:document/w:body/w:sdt/w:sdtContent/w:p/w:r[2]/w:instrText", u" BIBLIOGRAPHY ");
    assertXPath(pXmlDoc, "/w:document/w:body/w:sdt/w:sdtPr/w:docPartObj/w:docPartGallery", "val", u"Bibliographies");
    assertXPath(pXmlDoc, "/w:document/w:body/w:sdt/w:sdtPr/w:docPartObj/w:docPartUnique", 1);
}

CPPUNIT_TEST_FIXTURE(Test, testGenericTextField)
{
    loadAndReload("Unsupportedtextfields.docx");
    // fdo#75158 : This test case is to verify the unsupported textfields are exported properly.

    xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);
    xmlXPathObjectPtr pXmlObj = getXPathNode(pXmlDoc,"/w:document/w:body/w:p[2]/w:r[2]/w:instrText");
    xmlNodeSetPtr pXmlNodes = pXmlObj->nodesetval;
    xmlNodePtr pXmlNode = pXmlNodes->nodeTab[0];
    OUString contents = OUString::createFromAscii(reinterpret_cast<char*>((pXmlNode->children[0]).content));
    CPPUNIT_ASSERT(contents.match(" PRINTDATE "));
    xmlXPathFreeObject(pXmlObj);
}

CPPUNIT_TEST_FIXTURE(Test, test_FieldType)
{
    loadAndReload("99_Fields.docx");
    xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);
    // Checking for three field types (BIBLIOGRAPHY, BIDIOUTLINE, CITATION) in sequence
    assertXPath(pXmlDoc, "/w:document/w:body/w:p[2]/w:r[2]/w:instrText");
    assertXPath(pXmlDoc, "/w:document/w:body/w:p[3]/w:r[2]/w:instrText");
    assertXPath(pXmlDoc, "/w:document/w:body/w:p[4]/w:sdt/w:sdtContent/w:r[2]/w:instrText");
}

CPPUNIT_TEST_FIXTURE(Test, testCitation)
{
    loadAndReload("FDO74775.docx");
    xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);
    assertXPathContent(pXmlDoc, "/w:document/w:body/w:p[1]/w:sdt/w:sdtContent/w:r[2]/w:instrText", u" CITATION Kra06 \\l 1033 ");
    assertXPathContent(pXmlDoc, "/w:document/w:body/w:p[1]/w:sdt/w:sdtContent/w:r[4]/w:t", u"(Kramer & Chen, 2006)");
}

CPPUNIT_TEST_FIXTURE(Test, testHyperLinkTagEnded)
{
    loadAndReload("fdo76316.docx");
    /* XML tag <w:hyperlink> was not getting closed when its inside another
     * <w:hyperlink> tag.
     */
    xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);

    CPPUNIT_ASSERT(pXmlDoc);
    assertXPath(pXmlDoc, "/w:document[1]/w:body[1]/w:tbl[1]/w:tr[2]/w:tc[1]/w:tbl[1]/w:tr[1]/w:tc[1]/w:tbl[1]/w:tr[7]/w:tc[1]/w:tbl[1]/w:tr[2]/w:tc[6]/w:tbl[1]/w:tr[1]/w:tc[1]/w:p[1]/w:hyperlink[1]/w:hyperlink[1]",1);
}

CPPUNIT_TEST_FIXTURE(Test, testFDO76163 )
{
    loadAndReload("fdo76163.docx");
    xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);
    //docx file after RT is getting corrupted.
    assertXPath ( pXmlDoc, "/w:document/w:body/w:p[2]/w:hyperlink/w:r[10]/w:fldChar", "fldCharType", u"end" );
}

CPPUNIT_TEST_FIXTURE(Test, testFDO78659)
{
    loadAndReload("fdo78659.docx");
    xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);

    assertXPath(pXmlDoc, "/w:document[1]/w:body[1]/w:tbl[1]/w:tr[1]/w:tc[1]/w:p[1]/w:hyperlink[1]/w:r[3]/w:fldChar[1]", 0);
}

CPPUNIT_TEST_FIXTURE(Test, testFDO78654 )
{
    loadAndReload("fdo78654.docx");
    xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);
    // In case of two "Hyperlink" tags in one paragraph and one of them
    // contains "PAGEREF" field then field end tag was missing from hyperlink.
    assertXPath ( pXmlDoc, "/w:document/w:body/w:sdt/w:sdtContent/w:p[2]/w:hyperlink[3]/w:r[5]/w:fldChar", "fldCharType", u"end" );
}


CPPUNIT_TEST_FIXTURE(Test, testfdo78599)
{
    loadAndReload("fdo78599.docx");
    xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);
    //docx file after RT is getting corrupted.
    assertXPath ( pXmlDoc, "/w:document/w:body/w:p[1]/w:hyperlink/w:r[6]/w:fldChar", "fldCharType", u"end" );

    // Check for automatic hyphenation
    xmlDocUniquePtr pSettingsXml = parseExport(u"word/settings.xml"_ustr);
    // This failed as w:settings had no w:autoHyphenation child.
    assertXPath(pSettingsXml, "/w:settings/w:autoHyphenation");
}

CPPUNIT_TEST_FIXTURE(Test, testfdo78886)
{
    loadAndReload("fdo78886.docx");
    xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);

    assertXPath(pXmlDoc, "/w:document[1]/w:body[1]/w:tbl[2]/w:tr[1]/w:tc[1]/w:p[1]/w:hyperlink[1]/w:r[2]/w:fldChar[1]", 0);
}

CPPUNIT_TEST_FIXTURE(Test, testFdo78910)
{
    loadAndReload("fdo78910.docx");
    xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);

    // This is to ensure that the fld starts and ends inside a hyperlink...
    assertXPath ( pXmlDoc, "//w:hyperlink[2]/w:r[1]/w:fldChar", "fldCharType", u"begin" );
    assertXPath ( pXmlDoc, "//w:hyperlink[2]/w:r[5]/w:fldChar", "fldCharType", u"end" );
}

CPPUNIT_TEST_FIXTURE(Test, testFDO78590)
{
    loadAndReload("FDO78590.docx");
    xmlDocUniquePtr pXmlDoc = parseExport("word/document.xml");

    // This is to ensure that the fld starts and ends inside a hyperlink...
    assertXPath ( pXmlDoc, "/w:document/w:body/w:p[1]/w:pPr/w:framePr", "w", u"9851" );
    assertXPath ( pXmlDoc, "/w:document/w:body/w:p[1]/w:pPr/w:framePr", "h", u"1669" );
}

CPPUNIT_TEST_FIXTURE(Test, testSdtCitationRun)
{
    loadAndReload("sdt-citation-run.docx");
    // The problem was that the SDT was around the whole paragraph, not only around the citation field.
    xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);
    assertXPathContent(pXmlDoc, "/w:document/w:body/w:p/w:r[1]/w:t", u"Before sdt.");
    assertXPathContent(pXmlDoc, "/w:document/w:body/w:p/w:sdt/w:sdtContent/w:r/w:instrText", u" CITATION BBC11 \\l 1033 ");
    assertXPathContent(pXmlDoc, "/w:document/w:body/w:p/w:r[2]/w:t", u"After sdt.");
}

CPPUNIT_TEST_FIXTURE(Test, testParagraphSdt)
{
    loadAndReload("paragraph-sdt.docx");
    // The problem was that the SDT was around the run only, not the whole paragraph.
    xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);
    // The path to w:sdt contained a w:p.
    assertXPath(pXmlDoc, "/w:document/w:body/w:tbl/w:tr/w:tc/w:p/w:sdt");

    uno::Reference<text::XTextTablesSupplier> xTablesSupplier(mxComponent, uno::UNO_QUERY);
    uno::Reference<container::XIndexAccess> xTables(xTablesSupplier->getTextTables(), uno::UNO_QUERY);
    uno::Reference<text::XTextTable> xTable(xTables->getByIndex(0), uno::UNO_QUERY);
    uno::Reference<table::XCell> xCell = xTable->getCellByName(u"A1"_ustr);
    uno::Reference<container::XEnumerationAccess> xParagraphsAccess(xCell, uno::UNO_QUERY);
    uno::Reference<container::XEnumeration> xParagraphs = xParagraphsAccess->createEnumeration();
    uno::Reference<container::XEnumerationAccess> xParagraph(xParagraphs->nextElement(),
                                                            uno::UNO_QUERY);
    uno::Reference<container::XEnumeration> xPortions = xParagraph->createEnumeration();
    uno::Reference<beans::XPropertySet> xTextPortion(xPortions->nextElement(), uno::UNO_QUERY);
    OUString aPortionType;
    xTextPortion->getPropertyValue(u"TextPortionType"_ustr) >>= aPortionType;
    CPPUNIT_ASSERT_EQUAL(u"ContentControl"_ustr, aPortionType);
    uno::Reference<text::XTextContent> xContentControl;
    xTextPortion->getPropertyValue(u"ContentControl"_ustr) >>= xContentControl;
    uno::Reference<container::XEnumerationAccess> xContentControlEnumAccess(xContentControl, uno::UNO_QUERY);
    uno::Reference<container::XEnumeration> xContentControlEnum = xContentControlEnumAccess->createEnumeration();
    uno::Reference<text::XTextRange> xTextPortionRange(xContentControlEnum->nextElement(), uno::UNO_QUERY);
    CPPUNIT_ASSERT_EQUAL(u"Android Security Final Design Report"_ustr, xTextPortionRange->getString());
}

CPPUNIT_TEST_FIXTURE(Test, testTdf158661_blockSDT)
{
    loadAndReload("tdf158661_blockSDT.docx");
    uno::Reference<text::XTextTablesSupplier> xTablesSupplier(mxComponent, uno::UNO_QUERY);
    uno::Reference<container::XIndexAccess> xTables(xTablesSupplier->getTextTables(), uno::UNO_QUERY);
    uno::Reference<text::XTextTable> xTable(xTables->getByIndex(0), uno::UNO_QUERY);
    uno::Reference<table::XCell> xCell = xTable->getCellByName(u"A1"_ustr);
    uno::Reference<container::XEnumerationAccess> xParagraphsAccess(xCell, uno::UNO_QUERY);
    uno::Reference<container::XEnumeration> xParagraphs = xParagraphsAccess->createEnumeration();
    uno::Reference<container::XEnumerationAccess> xParagraph(xParagraphs->nextElement(),
                                                            uno::UNO_QUERY);
    uno::Reference<container::XEnumeration> xPortions = xParagraph->createEnumeration();
    uno::Reference<beans::XPropertySet> xTextPortion(xPortions->nextElement(), uno::UNO_QUERY);
    OUString aPortionType;
    xTextPortion->getPropertyValue(u"TextPortionType"_ustr) >>= aPortionType;
    CPPUNIT_ASSERT_EQUAL(u"ContentControl"_ustr, aPortionType);
    uno::Reference<text::XTextContent> xContentControl;
    xTextPortion->getPropertyValue(u"ContentControl"_ustr) >>= xContentControl;
    uno::Reference<beans::XPropertySet> xContentControlProps(xContentControl, uno::UNO_QUERY);
    uno::Reference<container::XEnumerationAccess> xContentControlEnumAccess(xContentControl, uno::UNO_QUERY);
    uno::Reference<container::XEnumeration> xContentControlEnum = xContentControlEnumAccess->createEnumeration();
    uno::Reference<text::XTextRange> xTextPortionRange(xContentControlEnum->nextElement(), uno::UNO_QUERY);
    CPPUNIT_ASSERT_EQUAL(u"Survey Strategy"_ustr, xTextPortionRange->getString());

    xTable.set(xTables->getByIndex(1), uno::UNO_QUERY);
    xCell = xTable->getCellByName(u"A1"_ustr);
    xParagraphsAccess.set(xCell, uno::UNO_QUERY);
    xParagraphs = xParagraphsAccess->createEnumeration();
    xParagraph.set(xParagraphs->nextElement(), uno::UNO_QUERY);
    xPortions = xParagraph->createEnumeration();
    xTextPortion.set(xPortions->nextElement(), uno::UNO_QUERY);
    xTextPortion->getPropertyValue(u"TextPortionType"_ustr) >>= aPortionType;
    CPPUNIT_ASSERT_EQUAL(u"ContentControl"_ustr, aPortionType);
    xTextPortion->getPropertyValue(u"ContentControl"_ustr) >>= xContentControl;
    xContentControlProps.set(xContentControl, uno::UNO_QUERY);
    xContentControlEnumAccess.set(xContentControl, uno::UNO_QUERY);
    xContentControlEnum = xContentControlEnumAccess->createEnumeration();
    xTextPortionRange.set(xContentControlEnum->nextElement(), uno::UNO_QUERY);
    CPPUNIT_ASSERT_EQUAL(u"Test"_ustr, xTextPortionRange->getString());
}

CPPUNIT_TEST_FIXTURE(Test, testSdt2Run)
{
    loadAndReload("sdt-2-para.docx");
    xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);
    // The problem was that <w:sdt> was closed after "first", not after "second", so the second assert failed.
    assertXPathContent(pXmlDoc, "/w:document/w:body/w:sdt/w:sdtContent/w:p[1]/w:r/w:t", u"first");
    assertXPathContent(pXmlDoc, "/w:document/w:body/w:sdt/w:sdtContent/w:p[2]/w:r/w:t", u"second");
    // Make sure the third paragraph is still outside <w:sdt>.
    assertXPathContent(pXmlDoc, "/w:document/w:body/w:p/w:r/w:t", u"third");
}

CPPUNIT_TEST_FIXTURE(Test, test2Id)
{
    loadAndReload("2-id.docx");
    xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);
    // This was 2, but only one w:id is allowed.
    assertXPath(pXmlDoc, "//w:sdtPr/w:id", 1);
}

CPPUNIT_TEST_FIXTURE(Test, testTableStart2Sdt)
{
    loadAndReload("table-start-2-sdt.docx");
    xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);
    // w:docPartGallery should be a child of <w:docPartObj>, make sure it's not a child of w:text.
    assertXPath(pXmlDoc, "//w:sdt/w:sdtPr/w:text/w:docPartGallery", 0);
}

CPPUNIT_TEST_FIXTURE(Test, testSdtDateDuplicate)
{
    auto verify = [this](bool bIsExport = false) {
        if (bIsExport)
        {
            xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);
            // Single <w:sdt> was exported as 2 <w:sdt> elements.
            assertXPath(pXmlDoc, "//w:sdt", 1);
            uno::Reference<text::XTextTablesSupplier> xTablesSupplier(mxComponent, uno::UNO_QUERY);
            uno::Reference<container::XIndexAccess> xTables(xTablesSupplier->getTextTables(), uno::UNO_QUERY);
            uno::Reference<text::XTextTable> xTable(xTables->getByIndex(0), uno::UNO_QUERY);
            uno::Reference<table::XCell> xCell = xTable->getCellByName(u"A1"_ustr);
            uno::Reference<container::XEnumerationAccess> xParagraphsAccess(xCell, uno::UNO_QUERY);
            uno::Reference<container::XEnumeration> xParagraphs = xParagraphsAccess->createEnumeration();
            uno::Reference<container::XEnumerationAccess> xParagraph(xParagraphs->nextElement(),
                                                                 uno::UNO_QUERY);
            uno::Reference<container::XEnumeration> xPortions = xParagraph->createEnumeration();
            uno::Reference<beans::XPropertySet> xTextPortion(xPortions->nextElement(), uno::UNO_QUERY);
            OUString aPortionType;
            xTextPortion->getPropertyValue(u"TextPortionType"_ustr) >>= aPortionType;
            CPPUNIT_ASSERT_EQUAL(u"ContentControl"_ustr, aPortionType);
            uno::Reference<text::XTextContent> xContentControl;
            xTextPortion->getPropertyValue(u"ContentControl"_ustr) >>= xContentControl;
            uno::Reference<beans::XPropertySet> xContentControlProps(xContentControl, uno::UNO_QUERY);
            bool bDate{};
            xContentControlProps->getPropertyValue(u"Date"_ustr) >>= bDate;
            CPPUNIT_ASSERT(bDate);
            uno::Reference<container::XEnumerationAccess> xContentControlEnumAccess(xContentControl, uno::UNO_QUERY);
            uno::Reference<container::XEnumeration> xContentControlEnum = xContentControlEnumAccess->createEnumeration();
            uno::Reference<text::XTextRange> xTextPortionRange(xContentControlEnum->nextElement(), uno::UNO_QUERY);
            CPPUNIT_ASSERT_EQUAL(u"4/26/2012"_ustr, xTextPortionRange->getString());
        }
        else
        {
            SwDoc* pDoc = getSwDoc();
            IDocumentMarkAccess* pMarkAccess = pDoc->getIDocumentMarkAccess();
            CPPUNIT_ASSERT_EQUAL(sal_Int32(2), pMarkAccess->getAllMarksCount());

            ::sw::mark::DateFieldmark* pFieldmark
                = dynamic_cast<::sw::mark::DateFieldmark*>(*pMarkAccess->getAllMarksBegin());
            CPPUNIT_ASSERT(pFieldmark);
            CPPUNIT_ASSERT_EQUAL(ODF_FORMDATE, pFieldmark->GetFieldname());
            CPPUNIT_ASSERT_EQUAL(u"4/26/2012"_ustr, pFieldmark->GetContent());
        }
    };

    createSwDoc("sdt-date-duplicate.docx");
    verify();
    saveAndReload(mpFilter);
    verify(/*bIsExport*/ true);
}

CPPUNIT_TEST_FIXTURE(Test, testFdo81492)
{
    loadAndReload("fdo81492.docx");
    xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);
    assertXPathContent(pXmlDoc, "/w:document/w:body/w:p[1]/w:r[6]/w:instrText", u"ADDIN EN.CITE.DATA");
}

CPPUNIT_TEST_FIXTURE(Test, testEditTime)
{
    loadAndReload("fdo81341.docx");
    /* Issue was LO was not able to Import and Export EditTime in seconds format.
     * It was supporting Time in "HH:MM" format. But if DOCX contains Time in seconds,
     * then LO was not able to display time in "HH:MM:SS" format.
     * While exporting LO was writing plain text instead of field entry.
     */
    xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);
    //Ensure that EditTime is written inside w:fldChar in "HH:MM:SS" format.
    assertXPath(pXmlDoc, "/w:document/w:body/w:p/w:r[1]/w:fldChar", "fldCharType", u"begin");
    assertXPath(pXmlDoc, "/w:document/w:body/w:p/w:r[3]/w:fldChar", "fldCharType", u"separate");
    assertXPathContent(pXmlDoc, "/w:document/w:body/w:p/w:r[4]/w:t", u"00:05");
    assertXPath(pXmlDoc, "/w:document/w:body/w:p/w:r[5]/w:fldChar", "fldCharType", u"end");
}

CPPUNIT_TEST_FIXTURE(Test, testFlyFieldmark)
{
    loadAndReload("fly_fieldmark.fodt");
    // the problem was that the flys were written after the field start
    xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);
    // run 1 contains 2 shapes, one was at-page, one was at-char
    assertXPath(pXmlDoc, "/w:document/w:body/w:p[1]/w:r[1]/mc:AlternateContent", 2);
    // run 2 contains the field start
    assertXPath(pXmlDoc, "/w:document/w:body/w:p[1]/w:r[2]/w:fldChar", "fldCharType", u"begin");
    // run 3 contains the field instruction text
    assertXPathContent(pXmlDoc, "/w:document/w:body/w:p[1]/w:r[3]/w:instrText", u" FORMTEXT ");
}

CPPUNIT_TEST_FIXTURE(Test, testFdo81945)
{
    loadAndReload("fdo81945.docx");
    xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);

    assertXPath(pXmlDoc, "//w:sdt//w:sdt", 0);
}

CPPUNIT_TEST_FIXTURE(Test, testfdo82123)
{
    loadAndReload("fdo82123.docx");
    xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);

    // make sure there is only one run inside first SDT after RT as in the Original file.
    assertXPath(pXmlDoc, "/w:document/w:body/w:tbl/w:tr/w:tc[2]/w:p/w:sdt[1]/w:sdtContent/w:r/w:t", 1);
    assertXPath(pXmlDoc, "/w:document/w:body/w:tbl/w:tr/w:tc[2]/w:p/w:r/w:drawing", 1);
}

CPPUNIT_TEST_FIXTURE(Test, testSdtBeforeField)
{
    loadAndReload("sdt-before-field.docx");
    xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);
    // Make sure the field doesn't sneak inside the SDT: the SDT should contain only a single run (there were 6 ones).
    assertXPath(pXmlDoc, "//w:p/w:sdt/w:sdtContent/w:r/w:t", 1);
    assertXPath(pXmlDoc, "//w:p/w:r/w:fldChar", 3);
}

CPPUNIT_TEST_FIXTURE(Test, testfdo81946)
{
    loadAndReload("fdo81946.docx");
    xmlDocUniquePtr pXmlDoc = parseExport(u"word/header1.xml"_ustr);
    // make sure AlternateContent should not present in sdt
    assertXPath(pXmlDoc, "/w:hdr[1]/w:p[1]/w:sdt[1]/w:sdtContent[1]/w:r[2]/mc:AlternateContent[1]",0);
}

CPPUNIT_TEST_FIXTURE(Test, testfdo82492)
{
    loadAndReload("fdo82492.docx");
    xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);

    // make sure there is only one run inside first SDT after RT as in the Original file.
    assertXPath(pXmlDoc, "/w:document/w:body/w:p/w:sdt[1]/w:sdtContent/w:r/w:t", 1);
    assertXPath(pXmlDoc, "/w:document/w:body/w:p/w:r/mc:AlternateContent", 1);
}

CPPUNIT_TEST_FIXTURE(Test, testSdtHeader)
{
    loadAndReload("sdt-header.docx");
    // Problem was that w:sdt elements in headers were lost on import.
    xmlDocUniquePtr pXmlDoc = parseExport(u"word/header2.xml"_ustr);
    // This was 0, w:sdt (and then w:date) was missing.
    assertXPath(pXmlDoc, "//w:sdt/w:sdtPr/w:date", 1);
}

CPPUNIT_TEST_FIXTURE(Test, testSdtCompanyMultipara)
{
    loadAndReload("sdt-company-multipara.docx");
    xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);
    // Here is just a simple text node, so there should be either one or zero paragraph
    // (in this case sdt element is inside paragraph)
    assertXPath(pXmlDoc, "//w:sdtContent/w:p", 0);
    assertXPath(pXmlDoc, "//w:sdtContent/w:r", 2);
}

CPPUNIT_TEST_FIXTURE(Test, testFixedDateFields)
{
    auto verify = [this]() {
        uno::Reference<text::XTextFieldsSupplier> xTextFieldsSupplier(mxComponent, uno::UNO_QUERY);
        uno::Reference<container::XEnumerationAccess> xFieldsAccess(xTextFieldsSupplier->getTextFields());
        uno::Reference<container::XEnumeration> xFields(xFieldsAccess->createEnumeration());
        uno::Reference<beans::XPropertySet> xField(xFields->nextElement(), uno::UNO_QUERY);

        // Check fixed property was imported and date value was parsed correctly
        CPPUNIT_ASSERT_EQUAL(true, getProperty<bool>(xField, u"IsFixed"_ustr));
        css::util::DateTime date = getProperty<css::util::DateTime>(xField, u"DateTimeValue"_ustr);
        CPPUNIT_ASSERT_EQUAL(sal_uInt16(24), date.Day);
        CPPUNIT_ASSERT_EQUAL(sal_uInt16(7), date.Month);
        CPPUNIT_ASSERT_EQUAL(sal_Int16(2014), date.Year);
    };

    createSwDoc("fixed-date-field.docx");
    verify();
    saveAndReload(mpFilter);
    verify();

    xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);
    // Previously, fixed fields were exported as static text ("Date (fixed)")
    // Check they are now exported correctly as fldChar with fldLock attribute
    assertXPath(pXmlDoc, "/w:document/w:body/w:p/w:r[1]/w:fldChar", "fldLock", u"true");
}

CPPUNIT_TEST_FIXTURE(Test, testToxmarkHyperlink)
{
    loadAndReload("toxmarkhyperlink.fodt");
    // test that export doesn't assert with overlapping fields / hyperlink attr
}

CPPUNIT_TEST_FIXTURE(Test, testOO34469)
{
    loadAndReload("ooo34469-1.odt");
    CPPUNIT_ASSERT_EQUAL(1, getPages());
    xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);
    assertXPath(pXmlDoc, "/w:document/w:body/w:p[1]/w:hyperlink[1]", "anchor", u"2.9.2.Creating_New_files|outline");
}

CPPUNIT_TEST_FIXTURE(Test, testOO39845)
{
    loadAndReload("ooo39845-7.odt");
    CPPUNIT_ASSERT_EQUAL(1, getPages());
    xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);
    assertXPath(pXmlDoc, "/w:document/w:body/w:p[1]/w:hyperlink[1]", "anchor", u"Figure4|graphic");
}

DECLARE_OOXMLEXPORT_TEST( testTdf85161, "tdf85161.docx" )
{
    CPPUNIT_ASSERT_EQUAL(u"Symbol"_ustr, getProperty<OUString>(getRun(getParagraph(1), 1), u"CharFontName"_ustr));
    CPPUNIT_ASSERT_EQUAL(OUString(u'\x5e'),getParagraph(1)->getString());
}

CPPUNIT_TEST_FIXTURE(Test, testTdf66401)
{
    loadAndReload("tdf66401.docx");
    xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);
    assertXPath(pXmlDoc, "/w:document/w:body/w:p[1]/w:r[3]/w:rPr/w:rFonts", 1);
    assertXPath(pXmlDoc, "/w:document/w:body/w:p[1]/w:r[3]/w:rPr/w:rFonts", "ascii", u"Arial Black");
    assertXPath(pXmlDoc, "/w:document/w:body/w:p[1]/w:r[3]/w:rPr/w:sz", "val", u"24");
    assertXPath(pXmlDoc, "/w:document/w:body/w:p[1]/w:r[9]/w:rPr/w:rFonts", 1);
    assertXPath(pXmlDoc, "/w:document/w:body/w:p[1]/w:r[9]/w:rPr/w:rFonts", "ascii", u"Arial Black");
    assertXPath(pXmlDoc, "/w:document/w:body/w:p[1]/w:r[9]/w:rPr/w:sz", "val", u"24");
}

CPPUNIT_TEST_FIXTURE(Test, testDateFieldInShape)
{
    auto verify = [this](bool bIsExport = false) {
    // This was crashed on export.
        if (bIsExport)
        {
            uno::Reference<text::XTextRange> xShape(getShape(1), uno::UNO_QUERY);
            uno::Reference<text::XText> xShapeText = xShape->getText();
            uno::Reference<beans::XPropertySet> xTextPortion(getRun(getParagraphOfText(1, xShapeText), 1), uno::UNO_QUERY);
            OUString aPortionType;
            xTextPortion->getPropertyValue(u"TextPortionType"_ustr) >>= aPortionType;
            CPPUNIT_ASSERT_EQUAL(u"ContentControl"_ustr, aPortionType);
            uno::Reference<text::XTextContent> xContentControl;
            xTextPortion->getPropertyValue(u"ContentControl"_ustr) >>= xContentControl;
            uno::Reference<beans::XPropertySet> xContentControlProps(xContentControl, uno::UNO_QUERY);
            bool bDate{};
            xContentControlProps->getPropertyValue(u"Date"_ustr) >>= bDate;
            CPPUNIT_ASSERT(bDate);
            uno::Reference<container::XEnumerationAccess> xContentControlEnumAccess(xContentControl, uno::UNO_QUERY);
            uno::Reference<container::XEnumeration> xContentControlEnum = xContentControlEnumAccess->createEnumeration();
            uno::Reference<text::XTextRange> xTextPortionRange(xContentControlEnum->nextElement(), uno::UNO_QUERY);
            CPPUNIT_ASSERT_EQUAL(u"Click here to enter a date."_ustr, xTextPortionRange->getString());
        }
        else
        {
            SwDoc* pDoc = getSwDoc();
            IDocumentMarkAccess* pMarkAccess = pDoc->getIDocumentMarkAccess();
            CPPUNIT_ASSERT_EQUAL(sal_Int32(2), pMarkAccess->getAllMarksCount());

            ::sw::mark::DateFieldmark* pFieldmark
                = dynamic_cast<::sw::mark::DateFieldmark*>(*pMarkAccess->getAllMarksBegin());
            CPPUNIT_ASSERT(pFieldmark);
            CPPUNIT_ASSERT_EQUAL(ODF_FORMDATE, pFieldmark->GetFieldname());
            CPPUNIT_ASSERT_EQUAL(u"Click here to enter a date."_ustr, pFieldmark->GetContent());
        }
    };

    createSwDoc("date_field_in_shape.docx");
    verify();
    saveAndReload(mpFilter);
    verify(/*bIsExport*/ true);
}

CPPUNIT_TEST_FIXTURE(Test, testDateFieldAtEndOfParagraph)
{
    auto verify = [this](bool bIsExport = false) {
        // Additional line end was added by import and it was crashed on export
        if (bIsExport)
        {
            uno::Reference<beans::XPropertySet> xTextPortion(getRun(getParagraph(2), 1), uno::UNO_QUERY);
            OUString aPortionType;
            xTextPortion->getPropertyValue(u"TextPortionType"_ustr) >>= aPortionType;
            CPPUNIT_ASSERT_EQUAL(u"ContentControl"_ustr, aPortionType);
            uno::Reference<text::XTextContent> xContentControl;
            xTextPortion->getPropertyValue(u"ContentControl"_ustr) >>= xContentControl;
            uno::Reference<beans::XPropertySet> xContentControlProps(xContentControl, uno::UNO_QUERY);
            bool bDate{};
            xContentControlProps->getPropertyValue(u"Date"_ustr) >>= bDate;
            CPPUNIT_ASSERT(bDate);
            uno::Reference<container::XEnumerationAccess> xContentControlEnumAccess(xContentControl, uno::UNO_QUERY);
            uno::Reference<container::XEnumeration> xContentControlEnum = xContentControlEnumAccess->createEnumeration();
            uno::Reference<text::XTextRange> xTextPortionRange(xContentControlEnum->nextElement(), uno::UNO_QUERY);
            CPPUNIT_ASSERT_EQUAL(u"Click here to enter a date."_ustr, xTextPortionRange->getString());
        }
        else
        {
            SwDoc* pDoc = getSwDoc();
            IDocumentMarkAccess* pMarkAccess = pDoc->getIDocumentMarkAccess();
            CPPUNIT_ASSERT_EQUAL(sal_Int32(2), pMarkAccess->getAllMarksCount());

            ::sw::mark::DateFieldmark* pFieldmark
                = dynamic_cast<::sw::mark::DateFieldmark*>(*pMarkAccess->getAllMarksBegin());
            CPPUNIT_ASSERT(pFieldmark);
            CPPUNIT_ASSERT_EQUAL(ODF_FORMDATE, pFieldmark->GetFieldname());
            CPPUNIT_ASSERT_EQUAL(u"Click here to enter a date."_ustr, pFieldmark->GetContent());
        }
    };

    createSwDoc("date_field_at_end_of_paragraph.docx");
    verify();
    saveAndReload(mpFilter);
    verify(/*bIsExport*/ true);
}

CPPUNIT_TEST_FIXTURE(Test, testDropDownFieldEntryLimit)
{
    auto verify = [this](bool bIsExport = false) {
        CPPUNIT_ASSERT_EQUAL(1, getPages());
        // In MSO, there is a limit of 25 for the items in a drop-down form field.
        // So we truncate the list of items to not exceed this limit.

        SwDoc* pDoc = getSwDoc();
        IDocumentMarkAccess* pMarkAccess = pDoc->getIDocumentMarkAccess();
        CPPUNIT_ASSERT_EQUAL(sal_Int32(1), pMarkAccess->getAllMarksCount());

        ::sw::mark::Fieldmark* pFieldmark
              = dynamic_cast<::sw::mark::Fieldmark*>(*pMarkAccess->getAllMarksBegin());
        CPPUNIT_ASSERT(pFieldmark);
        CPPUNIT_ASSERT_EQUAL(ODF_FORMDROPDOWN, pFieldmark->GetFieldname());

        const sw::mark::Fieldmark::parameter_map_t* const pParameters = pFieldmark->GetParameters();
        auto pListEntries = pParameters->find(ODF_FORMDROPDOWN_LISTENTRY);
        CPPUNIT_ASSERT(bool(pListEntries != pParameters->end()));
        css::uno::Sequence<OUString> vListEntries;
        pListEntries->second >>= vListEntries;
        if (!bIsExport)
            CPPUNIT_ASSERT_EQUAL(sal_Int32(26), vListEntries.getLength());
        else
            CPPUNIT_ASSERT_EQUAL(sal_Int32(25), vListEntries.getLength());
    };

    createSwDoc("tdf126792.odt" );
    verify();
    saveAndReload(mpFilter);
    verify(/*bIsExport*/ true);
}

CPPUNIT_TEST_FIXTURE(Test, testTdf132185)
{
    loadAndReload("tdf132185.docx");
    xmlDocUniquePtr pXmlDoc = parseExport(u"word/footer2.xml"_ustr);
    // Since the default (without xml:space attribute) is to ignore leading and trailing spaces,
    // " PAGE \\* roman " will get imported as "PAGE \\* roman". This is also valid, and must be
    // treated accordingly. "roman" was ignored before the fix, exporting only " PAGE ".
    assertXPathContent(pXmlDoc, "/w:ftr/w:p/w:r[2]/w:instrText", u" PAGE \\* roman ");
}

CPPUNIT_TEST_FIXTURE(Test, testConditionalText)
{
    loadAndReload("conditional-text.fodt");
    // Load a document which has a conditional text field in it.
    xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);
    std::u16string_view aExpected(u" IF 1 < 2 \"True\" \"- 'False' -\"");

    // Without the accompanying fix in place, this test would have failed with:
    // - Expression: xmlXPathNodeSetGetLength(pXmlNodes) > 0
    // - In <...>, XPath '/w:document/w:body/w:p/w:r[2]/w:instrText' not found
    // i.e. the field was lost on export.
    assertXPathContent(pXmlDoc, "/w:document/w:body/w:p/w:r[2]/w:instrText", aExpected);
}

CPPUNIT_TEST_FIXTURE(Test, testConditionalText2)
{
    loadAndReload("tdf114537_conditional-text2.doc");
    // Load a document which has a conditional text field in it.
    xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);
    std::u16string_view aExpected(u" IF 1 = 1 \"test1\" \"test2\"");
    assertXPathContent(pXmlDoc, "/w:document/w:body/w:p/w:r[2]/w:instrText", aExpected);

    getParagraph(1, u"test1"_ustr);
}

CPPUNIT_TEST_FIXTURE(Test, testConditionalText3)
{
    loadAndReload("tdf125038_conditionalText3.docx");
    // It is unclear what the purpose of all the conditional IF statements is,
    // but the end result should not produce any output.
    // Previously, the fields were being displayed as text
    // instead of as building blocks for the IF field condition.
    CPPUNIT_ASSERT_EQUAL(sal_Int32(-1), getParagraph(1)->getString().indexOf('2'));
}

CPPUNIT_TEST_FIXTURE(Test, testConditionalText4)
{
    loadAndReload("tdf125038_conditionalText_DOCPROPERTY.docx");
    getParagraph(1, u"customized field"_ustr);
}

CPPUNIT_TEST_FIXTURE(Test, testTdf142464_ampm)
{
    auto verify = [this]() {
        css::uno::Reference<css::text::XTextFieldsSupplier> xTextFieldsSupplier(
            mxComponent, css::uno::UNO_QUERY_THROW);
        auto xFieldsAccess(xTextFieldsSupplier->getTextFields());
        auto xFields(xFieldsAccess->createEnumeration());
        css::uno::Reference<css::text::XTextField> xField(xFields->nextElement(),
                                                          css::uno::UNO_QUERY_THROW);

        // Without the fix in place, this would have failed with:
        //   - Expected: 12:32 PM
        //   - Actual  : 12:32 a12/p12
        CPPUNIT_ASSERT_EQUAL(u"12:32 PM"_ustr, xField->getPresentation(false));
    };

    createSwDoc("tdf142464_ampm.docx");
    verify();
    saveAndReload(mpFilter);
    verify();

    xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);
    // Without the fix in place, this would have failed with:
    //   - Expected:  DATE \@"H:mm\ AM/PM"
    //   - Actual  :  DATE \@"H:mm' a'M'/p'M"
    // i.e., the AM/PM would be treated as literal 'a' and 'p' followed by a month code
    assertXPathContent(pXmlDoc, "/w:document/w:body/w:p/w:r[2]/w:instrText",
                       u" DATE \\@\"H:mm\\ AM/PM\" ");
}

CPPUNIT_TEST_FIXTURE(Test, testSdtDatePicker)
{
    // Check that roundtrip for date picker field does not lose essential data
    loadAndSave("test_sdt_datepicker.docx");
    xmlDocUniquePtr pXmlDoc = parseExport(u"word/document.xml"_ustr);

    // Placeholder is here
    OUString sDocPart = getXPath(pXmlDoc, "/w:document/w:body/w:p/w:sdt/w:sdtPr/w:placeholder/w:docPart", "val");
    CPPUNIT_ASSERT_EQUAL(u"DefaultPlaceholder_-1854013437"_ustr, sDocPart);

    // Ensure that we have data binding stuff
    OUString sDBprefix = getXPath(pXmlDoc, "/w:document/w:body/w:p/w:sdt/w:sdtPr/w:dataBinding", "prefixMappings");
    CPPUNIT_ASSERT_EQUAL(u"xmlns:ns0='http://schemas.microsoft.com/vsto/samples' "_ustr, sDBprefix);

    OUString sDBxpath = getXPath(pXmlDoc, "/w:document/w:body/w:p/w:sdt/w:sdtPr/w:dataBinding", "xpath");
    CPPUNIT_ASSERT_EQUAL(u"/ns0:employees[1]/ns0:employee[1]/ns0:hireDate[1]"_ustr, sDBxpath);

    OUString sDBstoreid = getXPath(pXmlDoc, "/w:document/w:body/w:p/w:sdt/w:sdtPr/w:dataBinding", "storeItemID");
    CPPUNIT_ASSERT_EQUAL(u"{241A8A02-7FFD-488D-8827-63FBE74E8BC9}"_ustr, sDBstoreid);

    OUString sColor = getXPath(pXmlDoc, "/w:document/w:body/w:p/w:sdt/w:sdtPr/w15:color", "val");
    CPPUNIT_ASSERT_EQUAL(u"008000"_ustr, sColor);
}

CPPUNIT_TEST_FIXTURE(Test, testContentControlGrabBag)
{
    // Given a document with a <w:sdt> tag:
    createSwDoc("content-control-grab-bag.docx");

    // When exporting that document back to DOCX:
    // Then make sure that completes without an assertion failure, which would mean not-well-formed
    // output was produced:
    save(u"Office Open XML Text"_ustr);
}

CPPUNIT_TEST_FIXTURE(Test, testContentControlShape)
{
    // Given a document with a <w:sdt> tag:
    createSwDoc("content-control-shape.docx");

    // When exporting that document back to DOCX:
    // Then make sure that completes without an assertion failure, which would mean not-well-formed
    // output was produced, since the <w:sdt> was conditional but the </w:sdt> was unconditional:
    save(u"Office Open XML Text"_ustr);
}

CPPUNIT_TEST_FIXTURE(Test, testTdf104823)
{
    // Test how we can roundtrip sdt plain text with databindings support
    createSwDoc("tdf104823.docx");

    // First paragraph: content from core properties
    uno::Reference<text::XTextRange> xParagraph1 = getParagraph(1);
    auto xContentControl1 = getProperty<uno::Reference<text::XText>>(getRun(xParagraph1, 2), u"ContentControl"_ustr);
    // Check field value (it should be value from data source) and set new
    CPPUNIT_ASSERT_EQUAL(u"True Core Property Value"_ustr, xContentControl1->getString());
    xContentControl1->setString(u"New Core Property Value"_ustr);

    // Third paragraph: content from custom properties
    uno::Reference<text::XTextRange> xParagraph3 = getParagraph(3);
    auto xContentControl3 = getProperty<uno::Reference<text::XText>>(getRun(xParagraph3, 2), u"ContentControl"_ustr);
    // Check field value (it should be value from data source) and set new
    CPPUNIT_ASSERT_EQUAL(u"True Custom XML Value"_ustr, xContentControl3->getString());
    xContentControl3->setString(u"New Custom XML Value"_ustr);

    // Second paragraph: content from extended properties
    uno::Reference<text::XTextRange> xParagraph2 = getParagraph(2);
    auto xContentControl2 = getProperty<uno::Reference<text::XText>>(getRun(xParagraph2, 2), u"ContentControl"_ustr);
    // Check field value (it should be value from data source) and set new
    CPPUNIT_ASSERT_EQUAL(u"True Extended Property Value"_ustr, xContentControl2->getString());
    xContentControl2->setString(u"New Extended Property Value"_ustr);

    // Save and check saved data
    save(u"Office Open XML Text"_ustr);
    xmlDocUniquePtr pXmlCustomPropsDoc = parseExport(u"customXml/item1.xml"_ustr);
    CPPUNIT_ASSERT(pXmlCustomPropsDoc);
    // FIXME: strange it won't run simple /employees/employee/name xpath query. Does not like namespaces?
    assertXPathContent(pXmlCustomPropsDoc, "//*/*[local-name()='name']", u"New Custom XML Value");

    xmlDocUniquePtr pXmlAppPropsDoc = parseExport(u"docProps/app.xml"_ustr);
    CPPUNIT_ASSERT(pXmlAppPropsDoc);
    // TODO: extended properties are not written yet
    assertXPathContent(pXmlAppPropsDoc, "//*/*[local-name()='Company']", u"True Extended Property Value");

    xmlDocUniquePtr pXmlCorePropsDoc = parseExport(u"docProps/core.xml"_ustr);
    CPPUNIT_ASSERT(pXmlCorePropsDoc);
    // TODO: core properties are not written yet
    assertXPathContent(pXmlCorePropsDoc, "/cp:coreProperties/dc:creator", u"True Core Property Value");
}

CPPUNIT_PLUGIN_IMPLEMENT();

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
