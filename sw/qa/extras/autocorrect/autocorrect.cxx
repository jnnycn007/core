/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <swmodeltestbase.hxx>

#include <comphelper/configuration.hxx>
#include <docsh.hxx>
#include <editeng/acorrcfg.hxx>
#include <ndtxt.hxx>
#include <officecfg/Office/Common.hxx>
#include <swacorr.hxx>
#include <unotools/syslocaleoptions.hxx>
#include <unotxdoc.hxx>
#include <wrtsh.hxx>

// If you want to add a test for a language that doesn't exists yet
// copy an existing document and adapt "fo:language" and "fo:country"

class SwAutoCorrectTest : public SwModelTestBase
{
public:
    SwAutoCorrectTest()
        : SwModelTestBase(u"/sw/qa/extras/autocorrect/data/"_ustr)
    {
    }
};

CPPUNIT_TEST_FIXTURE(SwAutoCorrectTest, testTdf92029)
{
    createSwDoc("nl-BE.fodt");

    SwXTextDocument* pTextDoc = dynamic_cast<SwXTextDocument*>(mxComponent.get());

    // Without the fix in place, this test would have failed with
    // - Expected: Ik ben ’s morgens opgestaan
    // - Actual  : Ik ben ‘s morgens opgestaan
    emulateTyping(*pTextDoc, u"Ik ben 's morgens opgestaan");
    OUString sReplaced(u"Ik ben ’s morgens opgestaan"_ustr);
    CPPUNIT_ASSERT_EQUAL(sReplaced, getParagraph(1)->getString());
}

CPPUNIT_TEST_FIXTURE(SwAutoCorrectTest, testTdf158703)
{
    createSwDoc("fr-FR.fodt");
    SwXTextDocument* pTextDoc = dynamic_cast<SwXTextDocument*>(mxComponent.get());
    CPPUNIT_ASSERT(pTextDoc);

    // Typing ":" after the spaces should start auto-correction, which is expected to
    // remove the spaces, and insert an NBSP instead. It must not crash.
    emulateTyping(*pTextDoc, u"Foo             :");
    CPPUNIT_ASSERT_EQUAL(u"Foo\u00A0:"_ustr, getParagraph(1)->getString());
}

CPPUNIT_TEST_FIXTURE(SwAutoCorrectTest, testTdf38394)
{
    createSwDoc("fr-FR.fodt");
    SwXTextDocument* pTextDoc = dynamic_cast<SwXTextDocument*>(mxComponent.get());
    CPPUNIT_ASSERT(pTextDoc);

    // testing autocorrect of French l'" -> l'« (instead of l'»)
    emulateTyping(*pTextDoc, u"l'\"");
    CPPUNIT_ASSERT_EQUAL(u"l\u2019« "_ustr, getParagraph(1)->getString());
}

CPPUNIT_TEST_FIXTURE(SwAutoCorrectTest, testTdf132301)
{
    createSwDoc("fr-FR.fodt");
    SwXTextDocument* pTextDoc = dynamic_cast<SwXTextDocument*>(mxComponent.get());
    CPPUNIT_ASSERT(pTextDoc);

    // autocorrect of qu'«
    emulateTyping(*pTextDoc, u" qu'\"");
    CPPUNIT_ASSERT_EQUAL(u" qu\u2019« "_ustr, getParagraph(1)->getString());
}

CPPUNIT_TEST_FIXTURE(SwAutoCorrectTest, tdfTdf44293)
{
    createSwDoc("pt-BR.fodt");
    SwXTextDocument* pTextDoc = dynamic_cast<SwXTextDocument*>(mxComponent.get());
    CPPUNIT_ASSERT(pTextDoc);

    emulateTyping(*pTextDoc, u"1a 1o ");
    CPPUNIT_ASSERT_EQUAL(u"1.a 1.o "_ustr, getParagraph(1)->getString());
    emulateTyping(*pTextDoc, u"1ra 1ro ");
    CPPUNIT_ASSERT_EQUAL(u"1.a 1.o 1.a 1.o "_ustr, getParagraph(1)->getString());
    emulateTyping(*pTextDoc, u"43as 43os 43ras 43ros ");
    CPPUNIT_ASSERT_EQUAL(u"1.a 1.o 1.a 1.o 43.as 43.os 43.as 43.os "_ustr,
                         getParagraph(1)->getString());
}

CPPUNIT_TEST_FIXTURE(SwAutoCorrectTest, testTdf151801)
{
    Resetter resetter([]() {
        std::shared_ptr<comphelper::ConfigurationChanges> pBatch(
            comphelper::ConfigurationChanges::create());
        officecfg::Office::Common::AutoCorrect::SingleQuoteAtStart::set(0, pBatch);
        officecfg::Office::Common::AutoCorrect::SingleQuoteAtEnd::set(0, pBatch);
        return pBatch->commit();
    });
    // Set Single Quotes › and ‹
    std::shared_ptr<comphelper::ConfigurationChanges> pBatch(
        comphelper::ConfigurationChanges::create());
    officecfg::Office::Common::AutoCorrect::SingleQuoteAtStart::set(8250, pBatch);
    officecfg::Office::Common::AutoCorrect::SingleQuoteAtEnd::set(8249, pBatch);
    pBatch->commit();

    createSwDoc("de-DE.fodt");
    SwXTextDocument* pTextDoc = dynamic_cast<SwXTextDocument*>(mxComponent.get());

    // Single starting quote: 'word -> ›word
    emulateTyping(*pTextDoc, u"'word");
    OUString sReplaced(u"\u203Aword"_ustr);
    CPPUNIT_ASSERT_EQUAL(sReplaced, getParagraph(1)->getString());
    // Single ending quote: ›word' -> ›word‹
    emulateTyping(*pTextDoc, u"'");
    sReplaced += u"\u2039";
    CPPUNIT_ASSERT_EQUAL(sReplaced, getParagraph(1)->getString());
    // Use apostrophe without preceding starting quote: word' -> word’
    emulateTyping(*pTextDoc, u" word'");
    sReplaced += u" word\u2019";
    CPPUNIT_ASSERT_EQUAL(sReplaced, getParagraph(1)->getString());
}

CPPUNIT_TEST_FIXTURE(SwAutoCorrectTest, testTdf128860)
{
    createSwDoc("de-DE.fodt");
    SwXTextDocument* pTextDoc = dynamic_cast<SwXTextDocument*>(mxComponent.get());

    // Second level ending quote: ‚word' -> ,word‘
    emulateTyping(*pTextDoc, u",word'");
    OUString sReplaced2(u",word\u2019"_ustr);
    CPPUNIT_ASSERT_EQUAL(sReplaced2, getParagraph(1)->getString());
    // Us apostrophe without preceding starting quote: word' -> word’
    emulateTyping(*pTextDoc, u" word'");
    sReplaced2 += u" word\u2019";
    CPPUNIT_ASSERT_EQUAL(sReplaced2, getParagraph(1)->getString());
    // But only after letters: word.' -> word.‘
    emulateTyping(*pTextDoc, u" word.'");
    sReplaced2 += u" word.‘";
    CPPUNIT_ASSERT_EQUAL(sReplaced2, getParagraph(1)->getString());
}

CPPUNIT_TEST_FIXTURE(SwAutoCorrectTest, testTdf123786)
{
    createSwDoc("ru-RU.fodt");

    // On Windows, it will detect that system input language is en-US (despite "typing" e.g. Cyrillic characters),
    // and will change Russian into English (US); in the latter language,
    // the replacement from single quote will not become “, but ’.
    SvtSysLocaleOptions aOptions;
    aOptions.SetIgnoreLanguageChange(true);
    aOptions.Commit();

    SwXTextDocument* pTextDoc = dynamic_cast<SwXTextDocument*>(mxComponent.get());
    // Second level ending quote: „word' -> „word“
    emulateTyping(*pTextDoc, u"„слово'");
    OUString sReplaced(u"„слово“"_ustr);
    CPPUNIT_ASSERT_EQUAL(sReplaced, getParagraph(1)->getString());
    // Us apostrophe without preceding starting quote: word' -> word’
    emulateTyping(*pTextDoc, u" слово'");
    sReplaced += u" слово’";
    CPPUNIT_ASSERT_EQUAL(sReplaced, getParagraph(1)->getString());
    // But only after letters: word.' -> word.“
    emulateTyping(*pTextDoc, u" слово.'");
    sReplaced += u" слово.“";
    CPPUNIT_ASSERT_EQUAL(sReplaced, getParagraph(1)->getString());
}

CPPUNIT_TEST_FIXTURE(SwAutoCorrectTest, testTdf106164)
{
    createSwDoc(); // Default lang is en-US
    SwXTextDocument& rTextDoc = dynamic_cast<SwXTextDocument&>(*mxComponent);

    // Testing autocorrect of we're -> We're on start of first paragraph
    emulateTyping(rTextDoc, u"we're ");
    CPPUNIT_ASSERT_EQUAL(u"We\u2019re "_ustr, getParagraph(1)->getString());
}

CPPUNIT_TEST_FIXTURE(SwAutoCorrectTest, testTdf59666)
{
    createSwDoc(); // Default lang is en-US
    SwXTextDocument& rTextDoc = dynamic_cast<SwXTextDocument&>(*mxComponent);

    // Testing missing autocorrect of single Greek letters
    emulateTyping(rTextDoc, u"π ");
    CPPUNIT_ASSERT_EQUAL(u"\u03C0 "_ustr, getParagraph(1)->getString());
}

CPPUNIT_TEST_FIXTURE(SwAutoCorrectTest, testTdf74363)
{
    createSwDoc(); // Default lang is en-US
    SwXTextDocument& rTextDoc = dynamic_cast<SwXTextDocument&>(*mxComponent);

    //testing autocorrect of initial capitals on start of first paragraph
    //Inserting one all-lowercase word into the first paragraph
    emulateTyping(rTextDoc, u"testing ");
    //The word should be capitalized due to autocorrect
    CPPUNIT_ASSERT_EQUAL(u"Testing "_ustr, getParagraph(1)->getString());
}

CPPUNIT_TEST_FIXTURE(SwAutoCorrectTest, testTdf155407)
{
    createSwDoc(); // Default lang is en-US
    SwXTextDocument& rTextDoc = dynamic_cast<SwXTextDocument&>(*mxComponent);

    emulateTyping(rTextDoc, u"Foo - 11’--’22 ");
    OUString sReplaced(u"Foo – 11’—’22 "_ustr);
    // Without the fix in place, this would fail with
    // - Expected: Foo – 11’—’22
    // - Actual  : Foo – 11’--’22
    CPPUNIT_ASSERT_EQUAL(sReplaced, getParagraph(1)->getString());

    emulateTyping(rTextDoc, u"Bar -- 111--222 ");
    sReplaced += u"Bar – 111–222 "_ustr;
    // Without the fix in place, this would fail with
    // - Expected: Bar – 111–222
    // - Actual  : Bar – 111-–22
    CPPUNIT_ASSERT_EQUAL(sReplaced, getParagraph(1)->getString());
}

CPPUNIT_TEST_FIXTURE(SwAutoCorrectTest, testTdf159797)
{
    createSwDoc(); // Default lang is en-US
    SwXTextDocument& rTextDoc = dynamic_cast<SwXTextDocument&>(*mxComponent);

    emulateTyping(rTextDoc, u"This - is replaced. - But this is not replaced ");
    // Without the fix in place, this would fail with
    // - Expected: This – is replaced. – But this is not replaced.
    // - Actual  : This – is replaced. - But this is not replaced.
    CPPUNIT_ASSERT_EQUAL(u"This – is replaced. – But this is not replaced "_ustr,
                         getParagraph(1)->getString());
}

CPPUNIT_TEST_FIXTURE(SwAutoCorrectTest, testTdf54409)
{
    createSwDoc(); // Default lang is en-US
    SwXTextDocument& rTextDoc = dynamic_cast<SwXTextDocument&>(*mxComponent);

    // testing autocorrect of "tset -> "test with typographical double quotation mark U+201C
    emulateTyping(rTextDoc, u"\"test ");
    OUString sReplaced(u"\u201Ctest "_ustr);
    CPPUNIT_ASSERT_EQUAL(sReplaced, getParagraph(1)->getString());
    // testing autocorrect of test" -> test" with typographical double quotation mark U+201D
    emulateTyping(rTextDoc, u"and tset\" ");
    OUString sReplaced2(sReplaced + u"and test\u201D ");
    CPPUNIT_ASSERT_EQUAL(sReplaced2, getParagraph(1)->getString());
    // testing autocorrect of "tset" -> "test" with typographical double quotation mark U+201C and U+201D
    emulateTyping(rTextDoc, u"\"tset\" ");
    OUString sReplaced3(sReplaced2 + u"\u201Ctest\u201D ");
    CPPUNIT_ASSERT_EQUAL(sReplaced3, getParagraph(1)->getString());
}

CPPUNIT_TEST_FIXTURE(SwAutoCorrectTest, testTdf108423)
{
    createSwDoc(); // Default lang is en-US
    SwXTextDocument& rTextDoc = dynamic_cast<SwXTextDocument&>(*mxComponent);

    // testing autocorrect of i' -> I' on start of first paragraph
    emulateTyping(rTextDoc, u"i'");
    // The word "i" should be capitalized due to autocorrect, followed by a typographical apostrophe
    OUString sIApostrophe(u"I\u2019"_ustr);
    CPPUNIT_ASSERT_EQUAL(sIApostrophe, getParagraph(1)->getString());
    emulateTyping(rTextDoc, u" i'");
    OUString sText(sIApostrophe + u" " + sIApostrophe);
    CPPUNIT_ASSERT_EQUAL(sText, getParagraph(1)->getString());
}

CPPUNIT_TEST_FIXTURE(SwAutoCorrectTest, testTdf57640)
{
    createSwDoc(); // Default lang is en-US
    SwXTextDocument& rTextDoc = dynamic_cast<SwXTextDocument&>(*mxComponent);

    // Without the fix in place, this test would have failed with
    // - Expected: ǅ
    // - Actual  : Ǆ
    emulateTyping(rTextDoc, u"ǆ  ");
    CPPUNIT_ASSERT_EQUAL(u"ǅ  "_ustr, getParagraph(1)->getString());
}

CPPUNIT_TEST_FIXTURE(SwAutoCorrectTest, testTdf62923)
{
    createSwDoc(); // Default lang is en-US
    SwXTextDocument& rTextDoc = dynamic_cast<SwXTextDocument&>(*mxComponent);

    OUString sReplaced(u"1–2 "_ustr);
    // Without the fix in place, this test would have failed with
    // - Expected: 1—2
    // - Actual  : 1–2
    emulateTyping(rTextDoc, u"1--2 ");
    CPPUNIT_ASSERT_EQUAL(sReplaced, getParagraph(1)->getString());

    sReplaced += u"a—b "_ustr;
    emulateTyping(rTextDoc, u"a--b ");
    CPPUNIT_ASSERT_EQUAL(sReplaced, getParagraph(1)->getString());
}

CPPUNIT_TEST_FIXTURE(SwAutoCorrectTest, testTdf42893)
{
    createSwDoc(); // Default lang is en-US
    SwXTextDocument& rTextDoc = dynamic_cast<SwXTextDocument&>(*mxComponent);

    OUString sReplaced(u"Foo. Bar "_ustr);
    emulateTyping(rTextDoc, u"foo. bar ");
    CPPUNIT_ASSERT_EQUAL(sReplaced, getParagraph(1)->getString());

    // Without the fix in place, this test would have failed with
    // - Expected: F.o.o. bar
    // - Actual  : F.o.o. Bar
    sReplaced += u"F.o.o. bar "_ustr;
    emulateTyping(rTextDoc, u"F.o.o. bar ");
    CPPUNIT_ASSERT_EQUAL(sReplaced, getParagraph(1)->getString());
}

CPPUNIT_TEST_FIXTURE(SwAutoCorrectTest, testTdf55693)
{
    createSwDoc(); // Default lang is en-US
    SwXTextDocument& rTextDoc = dynamic_cast<SwXTextDocument&>(*mxComponent);

    OUString sReplaced(u"Test-Test "_ustr);
    emulateTyping(rTextDoc, u"TEst-TEst ");
    CPPUNIT_ASSERT_EQUAL(sReplaced, getParagraph(1)->getString());

    sReplaced += u"Test/Test "_ustr;
    emulateTyping(rTextDoc, u"TEst/TEst ");
    CPPUNIT_ASSERT_EQUAL(sReplaced, getParagraph(1)->getString());

    sReplaced += u"Test. Test "_ustr;
    emulateTyping(rTextDoc, u"Test. test ");
    CPPUNIT_ASSERT_EQUAL(sReplaced, getParagraph(1)->getString());
}

CPPUNIT_TEST_FIXTURE(SwAutoCorrectTest, testFieldMark)
{
    // it's not possible to insert '\x01' with emulateTyping, use Insert instead
    // getString also strips it out, use GetText instead

    createSwDoc();
    SwDoc* pDoc = getSwDoc();
    SwWrtShell* const pWrtShell = pDoc->GetDocShell()->GetWrtShell();
    SwAutoCorrect corr(*SvxAutoCorrCfg::Get().GetAutoCorrect());

    // don't autocapitalize after a field mark
    pWrtShell->Insert(u"Test. \x01 test"_ustr);
    pWrtShell->AutoCorrect(corr, ' ');

    OUString sReplaced(u"Test. \x01 test "_ustr);
    CPPUNIT_ASSERT_EQUAL(sReplaced,
                         pWrtShell->GetCursor()->GetPointNode().GetTextNode()->GetText());

    // consider field contents as text for auto quotes
    pWrtShell->Insert(u"T\x01"_ustr);
    pWrtShell->AutoCorrect(corr, '"');

    sReplaced += u"T\x01\u201d"_ustr;
    CPPUNIT_ASSERT_EQUAL(sReplaced,
                         pWrtShell->GetCursor()->GetPointNode().GetTextNode()->GetText());
}

CPPUNIT_TEST_FIXTURE(SwAutoCorrectTest, testUnderlineWeight)
{
    Resetter resetter([]() {
        std::shared_ptr<comphelper::ConfigurationChanges> pBatch(
            comphelper::ConfigurationChanges::create());
        officecfg::Office::Common::AutoCorrect::ChangeUnderlineWeight::set(false, pBatch);
        officecfg::Office::Common::AutoCorrect::ChangeUnderlineWeight::set(false, pBatch);
        return pBatch->commit();
    });
    std::shared_ptr<comphelper::ConfigurationChanges> pBatch(
        comphelper::ConfigurationChanges::create());
    officecfg::Office::Common::AutoCorrect::ChangeUnderlineWeight::set(true, pBatch);
    officecfg::Office::Common::AutoCorrect::ChangeUnderlineWeight::set(true, pBatch);
    pBatch->commit();

    createSwDoc(); // Default lang is en-US
    SwXTextDocument& rTextDoc = dynamic_cast<SwXTextDocument&>(*mxComponent);

    //bold
    OUString sReplaced(u"Foo "_ustr);
    emulateTyping(rTextDoc, u"*foo* ");
    CPPUNIT_ASSERT_EQUAL(sReplaced, getParagraph(1)->getString());

    //underline
    sReplaced += u"foo "_ustr;
    emulateTyping(rTextDoc, u"_foo_ ");
    CPPUNIT_ASSERT_EQUAL(sReplaced, getParagraph(1)->getString());

    //italic
    sReplaced += u"foo "_ustr;
    emulateTyping(rTextDoc, u"/foo/ ");
    CPPUNIT_ASSERT_EQUAL(sReplaced, getParagraph(1)->getString());

    //strikeout
    sReplaced += u"foo "_ustr;
    emulateTyping(rTextDoc, u"-foo- ");
    CPPUNIT_ASSERT_EQUAL(sReplaced, getParagraph(1)->getString());
}

CPPUNIT_TEST_FIXTURE(SwAutoCorrectTest, testTdf133524)
{
    createSwDoc("hu-HU.fodt");
    SwXTextDocument* pTextDoc = dynamic_cast<SwXTextDocument*>(mxComponent.get());
    // 1. Testing autocorrect of >> and <<
    // Example: »word«
    emulateTyping(*pTextDoc, u">>");
    OUString sReplaced(u"»"_ustr);
    CPPUNIT_ASSERT_EQUAL(sReplaced, getParagraph(1)->getString());
    // <<
    emulateTyping(*pTextDoc, u"word<<");
    sReplaced += u"word«";
    CPPUNIT_ASSERT_EQUAL(sReplaced, getParagraph(1)->getString());
    // 2. Testing autocorrect of " to >> and << inside „...”
    // Example: „Sentence and »word«.”
    // opening primary level quote
    emulateTyping(*pTextDoc, u" \"");
    sReplaced += u" „";
    CPPUNIT_ASSERT_EQUAL(sReplaced, getParagraph(1)->getString());
    // opening second level quote
    emulateTyping(*pTextDoc, u"Sentence and \"");
    sReplaced += u"Sentence and »";
    CPPUNIT_ASSERT_EQUAL(sReplaced, getParagraph(1)->getString());
    // closing second level quote
    emulateTyping(*pTextDoc, u"word\"");
    sReplaced += u"word«";
    CPPUNIT_ASSERT_EQUAL(sReplaced, getParagraph(1)->getString());
    // closing primary level quote
    emulateTyping(*pTextDoc, u".\"");
    sReplaced += u".”";
    CPPUNIT_ASSERT_EQUAL(sReplaced, getParagraph(1)->getString());
}

CPPUNIT_TEST_FIXTURE(SwAutoCorrectTest, testTdf134940)
{
    createSwDoc("hu-HU.fodt");
    SwXTextDocument* pTextDoc = dynamic_cast<SwXTextDocument*>(mxComponent.get());

    // avoid premature replacement of "--" in "-->"
    emulateTyping(*pTextDoc, u" -->");
    // This was "–>" instead of "-->"
    CPPUNIT_ASSERT_EQUAL(u" -->"_ustr, getParagraph(1)->getString());
    emulateTyping(*pTextDoc, u" ");
    // This was "–>" instead of "→"
    CPPUNIT_ASSERT_EQUAL(u" → "_ustr, getParagraph(1)->getString());
}

CPPUNIT_TEST_FIXTURE(SwAutoCorrectTest, testTdf83037)
{
    createSwDoc("hu-HU.fodt");
    SwXTextDocument* pTextDoc = dynamic_cast<SwXTextDocument*>(mxComponent.get());

    emulateTyping(*pTextDoc, u"-> ");
    OUString sReplaced(u"→ "_ustr);
    CPPUNIT_ASSERT_EQUAL(sReplaced, getParagraph(1)->getString());
    emulateTyping(*pTextDoc, u"<- ");
    sReplaced += u"← ";
    CPPUNIT_ASSERT_EQUAL(sReplaced, getParagraph(1)->getString());
    emulateTyping(*pTextDoc, u"<-- ");
    sReplaced += u"← ";
    CPPUNIT_ASSERT_EQUAL(sReplaced, getParagraph(1)->getString());
    emulateTyping(*pTextDoc, u"<--> ");
    sReplaced += u"↔ ";
    CPPUNIT_ASSERT_EQUAL(sReplaced, getParagraph(1)->getString());
}

CPPUNIT_TEST_FIXTURE(SwAutoCorrectTest, testTdf133524_Romanian)
{
    createSwDoc("ro-RO.fodt");
    SwXTextDocument* pTextDoc = dynamic_cast<SwXTextDocument*>(mxComponent.get());
    // 1. Testing autocorrect of " to << and >> inside „...”
    // Example: „Sentence and «word».”
    // opening primary level quote
    emulateTyping(*pTextDoc, u"\"");
    OUString sReplaced(u"„"_ustr);
    CPPUNIT_ASSERT_EQUAL(sReplaced, getParagraph(1)->getString());
    // opening second level quote
    emulateTyping(*pTextDoc, u"Sentence and \"");
    sReplaced += u"Sentence and «";
    CPPUNIT_ASSERT_EQUAL(sReplaced, getParagraph(1)->getString());
    // closing second level quote
    emulateTyping(*pTextDoc, u"word\"");
    sReplaced += u"word»";
    CPPUNIT_ASSERT_EQUAL(sReplaced, getParagraph(1)->getString());
    // closing primary level quote
    emulateTyping(*pTextDoc, u".\"");
    sReplaced += u".”";
    CPPUNIT_ASSERT_EQUAL(sReplaced, getParagraph(1)->getString());
    // 2. Testing recognition of closing double quotation mark ”
    emulateTyping(*pTextDoc, u" \"");
    sReplaced += u" „";
    CPPUNIT_ASSERT_EQUAL(sReplaced, getParagraph(1)->getString());
    // 3. Testing recognition of alternative closing double quotation mark “
    emulateTyping(*pTextDoc, u"Alternative.“ \"");
    sReplaced += u"Alternative.“ „";
    CPPUNIT_ASSERT_EQUAL(sReplaced, getParagraph(1)->getString());
}

CPPUNIT_TEST_FIXTURE(SwAutoCorrectTest, testTdf158051)
{
    createSwDoc("el-GR.fodt");
    SwXTextDocument* pTextDoc = dynamic_cast<SwXTextDocument*>(mxComponent.get());

    // Without the fix in place, this test would have failed with
    // - Expected: Π Τάκης σ’ ευχαριστώ
    // - Actual  : Π Τάκης ς’ ευχαριστώ
    emulateTyping(*pTextDoc, u"π Τάκης σ’ ευχαριστώ");

#if !defined(_WIN32) // For some reason it fails on Window. the manual test works fine
    CPPUNIT_ASSERT_EQUAL(u"Π Τάκης σ’ ευχαριστώ"_ustr, getParagraph(1)->getString());
#endif
}

CPPUNIT_TEST_FIXTURE(SwAutoCorrectTest, testTdf162911)
{
    createSwDoc();
    SwXTextDocument* pTextDoc = dynamic_cast<SwXTextDocument*>(mxComponent.get());

    emulateTyping(*pTextDoc, u"foo@bar.com foo2@bar.com ");

    CPPUNIT_ASSERT_EQUAL(u"mailto:foo@bar.com"_ustr,
                         getProperty<OUString>(getRun(getParagraph(1), 1, u"foo@bar.com"_ustr),
                                               u"HyperLinkURL"_ustr));

    CPPUNIT_ASSERT_EQUAL(u""_ustr, getProperty<OUString>(getRun(getParagraph(1), 2, u" "_ustr),
                                                         u"HyperLinkURL"_ustr));

    CPPUNIT_ASSERT_EQUAL(u"mailto:foo2@bar.com"_ustr,
                         getProperty<OUString>(getRun(getParagraph(1), 3, u"foo2@bar.com"_ustr),
                                               u"HyperLinkURL"_ustr));

    // Without the fix in place, this would have crashed here
    dispatchCommand(mxComponent, u".uno:Undo"_ustr, {});

    CPPUNIT_ASSERT_EQUAL(u"mailto:foo@bar.com"_ustr,
                         getProperty<OUString>(getRun(getParagraph(1), 1, u"foo@bar.com"_ustr),
                                               u"HyperLinkURL"_ustr));

    CPPUNIT_ASSERT_EQUAL(u""_ustr,
                         getProperty<OUString>(getRun(getParagraph(1), 2, u" foo2@bar.com "_ustr),
                                               u"HyperLinkURL"_ustr));

    dispatchCommand(mxComponent, u".uno:Undo"_ustr, {});

    CPPUNIT_ASSERT_EQUAL(u"mailto:foo@bar.com"_ustr,
                         getProperty<OUString>(getRun(getParagraph(1), 1, u"foo@bar.com"_ustr),
                                               u"HyperLinkURL"_ustr));

    CPPUNIT_ASSERT_EQUAL(u""_ustr,
                         getProperty<OUString>(getRun(getParagraph(1), 2, u" foo2@bar.com"_ustr),
                                               u"HyperLinkURL"_ustr));

    dispatchCommand(mxComponent, u".uno:Undo"_ustr, {});

    CPPUNIT_ASSERT_EQUAL(u"mailto:foo@bar.com"_ustr,
                         getProperty<OUString>(getRun(getParagraph(1), 1, u"foo@bar.com"_ustr),
                                               u"HyperLinkURL"_ustr));

    CPPUNIT_ASSERT_EQUAL(u""_ustr,
                         getProperty<OUString>(getRun(getParagraph(1), 2, u" foo2@bar."_ustr),
                                               u"HyperLinkURL"_ustr));

    dispatchCommand(mxComponent, u".uno:Undo"_ustr, {});

    CPPUNIT_ASSERT_EQUAL(u"mailto:foo@bar.com"_ustr,
                         getProperty<OUString>(getRun(getParagraph(1), 1, u"foo@bar.com"_ustr),
                                               u"HyperLinkURL"_ustr));

    CPPUNIT_ASSERT_EQUAL(
        u""_ustr,
        getProperty<OUString>(getRun(getParagraph(1), 2, u" foo2@bar"_ustr), u"HyperLinkURL"_ustr));

    dispatchCommand(mxComponent, u".uno:Undo"_ustr, {});

    CPPUNIT_ASSERT_EQUAL(u"mailto:foo@bar.com"_ustr,
                         getProperty<OUString>(getRun(getParagraph(1), 1, u"foo@bar.com"_ustr),
                                               u"HyperLinkURL"_ustr));

    CPPUNIT_ASSERT_EQUAL(u""_ustr, getProperty<OUString>(getRun(getParagraph(1), 2, u" foo2@"_ustr),
                                                         u"HyperLinkURL"_ustr));

    dispatchCommand(mxComponent, u".uno:Undo"_ustr, {});

    CPPUNIT_ASSERT_EQUAL(u"mailto:foo@bar.com"_ustr,
                         getProperty<OUString>(getRun(getParagraph(1), 1, u"foo@bar.com"_ustr),
                                               u"HyperLinkURL"_ustr));

    CPPUNIT_ASSERT_EQUAL(u""_ustr, getProperty<OUString>(getRun(getParagraph(1), 2, u" foo2"_ustr),
                                                         u"HyperLinkURL"_ustr));

    dispatchCommand(mxComponent, u".uno:Undo"_ustr, {});

    CPPUNIT_ASSERT_EQUAL(u"mailto:foo@bar.com"_ustr,
                         getProperty<OUString>(getRun(getParagraph(1), 1, u"foo@bar.com"_ustr),
                                               u"HyperLinkURL"_ustr));

    CPPUNIT_ASSERT_EQUAL(u""_ustr, getProperty<OUString>(getRun(getParagraph(1), 2, u" "_ustr),
                                                         u"HyperLinkURL"_ustr));

    dispatchCommand(mxComponent, u".uno:Undo"_ustr, {});

    CPPUNIT_ASSERT_EQUAL(u""_ustr,
                         getProperty<OUString>(getRun(getParagraph(1), 1, u"foo@bar.com "_ustr),
                                               u"HyperLinkURL"_ustr));

    dispatchCommand(mxComponent, u".uno:Undo"_ustr, {});

    CPPUNIT_ASSERT_EQUAL(u""_ustr,
                         getProperty<OUString>(getRun(getParagraph(1), 1, u"foo@bar.com"_ustr),
                                               u"HyperLinkURL"_ustr));

    dispatchCommand(mxComponent, u".uno:Undo"_ustr, {});

    CPPUNIT_ASSERT_EQUAL(
        u""_ustr,
        getProperty<OUString>(getRun(getParagraph(1), 1, u"foo@bar."_ustr), u"HyperLinkURL"_ustr));

    dispatchCommand(mxComponent, u".uno:Undo"_ustr, {});

    CPPUNIT_ASSERT_EQUAL(
        u""_ustr,
        getProperty<OUString>(getRun(getParagraph(1), 1, u"foo@bar"_ustr), u"HyperLinkURL"_ustr));

    dispatchCommand(mxComponent, u".uno:Undo"_ustr, {});

    CPPUNIT_ASSERT_EQUAL(u""_ustr, getProperty<OUString>(getRun(getParagraph(1), 1, u"foo@"_ustr),
                                                         u"HyperLinkURL"_ustr));

    dispatchCommand(mxComponent, u".uno:Undo"_ustr, {});

    CPPUNIT_ASSERT_EQUAL(u""_ustr, getProperty<OUString>(getRun(getParagraph(1), 1, u"foo"_ustr),
                                                         u"HyperLinkURL"_ustr));

    dispatchCommand(mxComponent, u".uno:Undo"_ustr, {});

    CPPUNIT_ASSERT_EQUAL(u""_ustr, getProperty<OUString>(getRun(getParagraph(1), 1, u""_ustr),
                                                         u"HyperLinkURL"_ustr));
}

CPPUNIT_PLUGIN_IMPLEMENT();

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
