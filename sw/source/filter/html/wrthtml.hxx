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
#ifndef INCLUDED_SW_SOURCE_FILTER_HTML_WRTHTML_HXX
#define INCLUDED_SW_SOURCE_FILTER_HTML_WRTHTML_HXX

#include <memory>
#include <vector>
#include <set>
#include <string_view>
#include <map>
#include <optional>

#include <com/sun/star/container/XIndexContainer.hpp>
#include <com/sun/star/form/XForm.hpp>
#include <i18nlangtag/lang.h>
#include <comphelper/stl_types.hxx>
#include <o3tl/sorted_vector.hxx>
#include <o3tl/typed_flags_set.hxx>
#include <rtl/ref.hxx>
#include <svtools/htmlout.hxx>
#include <tools/fldunit.hxx>

#include <shellio.hxx>
#include <wrt_fn.hxx>
#include "htmlfly.hxx"

// some forward declarations
class SwFrameFormat;
class SwDrawFrameFormat;
class SwFormatINetFormat;
class SwFormatFootnote;
class SwStartNode;
class SwTableNode;
class SwPageDesc;
class SwNodeIndex;
class ImageMap;
class SwNumRule;
class SdrObject;
class SdrUnoObj;
class SvxBrushItem;
class SvxFontItem;
class SwHTMLNumRuleInfo;
class SwTextFootnote;
enum class HtmlTokenId : sal_Int16;
namespace utl { class TempFileNamed; }

extern const SwAttrFnTab aHTMLAttrFnTab;

#define HTML_PARSPACE (o3tl::toTwips(5, o3tl::Length::mm))

// flags for the output of any kind of frames
// BORDER only possible if OutHTML_Image
// ANYSIZE indicates, if also VAR_SIZE and MIN_SIZE values should be exported
// ABSSIZE indicates, if spacing and framing should be ignored
enum class HtmlFrmOpts {
    NONE        = 0,
    Align       = 1<<0,
    SAlign      = 1<<1,

    Width       = 1<<2,
    Height      = 1<<3,
    Size        = Width | Height,
    SWidth      = 1<<4,
    SHeight     = 1<<5,
    SSize       = SWidth | SHeight,
    AnySize     = 1<<6,
    AbsSize     = 1<<7,
    MarginSize  = 1<<8,

    Space       = 1<<9,
    SSpace      = 1<<10,

    Border      = 1<<11,
    SBorder     = 1<<12,
    SNoBorder   = 1<<13,

    SBackground = 1<<14,

    Name        = 1<<15,
    Alt         = 1<<16,
    BrClear     = 1<<17,
    SPixSize    = 1<<18,
    Id          = 1<<19,
    Dir         = 1<<20,
    /// The graphic frame is a replacement image of an OLE object.
    Replacement = 1<<21,

    GenImgAllMask = Alt | Size | AbsSize | Name,
    GenImgMask    = GenImgAllMask | Align | Space | BrClear
};
namespace o3tl {
    template<> struct typed_flags<HtmlFrmOpts> : is_typed_flags<HtmlFrmOpts, ((1<<22)-1)> {};
}

#define HTMLMODE_BLOCK_SPACER       0x00010000
#define HTMLMODE_FLOAT_FRAME        0x00020000
#define HTMLMODE_VERT_SPACER        0x00040000
#define HTMLMODE_NBSP_IN_TABLES     0x00080000
#define HTMLMODE_LSPACE_IN_NUMBER_BULLET 0x00100000
//was HTMLMODE_NO_BR_AT_PAREND    0x00200000
#define HTMLMODE_PRINT_EXT          0x00400000
#define HTMLMODE_ABS_POS_FLY        0x00800000
#define HTMLMODE_ABS_POS_DRAW       0x01000000
#define HTMLMODE_FLY_MARGINS        0x02000000
#define HTMLMODE_BORDER_NONE        0x04000000
#define HTMLMODE_FONT_GENERIC       0x08000000
#define HTMLMODE_FRSTLINE_IN_NUMBER_BULLET 0x10000000
#define HTMLMODE_NO_CONTROL_CENTERING 0x20000000

#define HTML_DLCOLL_DD 0x4000
#define HTML_DLCOLL_DT 0x8000

#define CSS1_FMT_ISTAG (USHRT_MAX)
#define CSS1_FMT_CMPREF (USHRT_MAX-1)
#define CSS1_FMT_SPECIAL (USHRT_MAX-1)

// the following flags only specify which descriptors, tags, options,
// and so on should be outputted
// bit 0,1,2
#define CSS1_OUTMODE_SPAN_NO_ON     0x0000U
#define CSS1_OUTMODE_SPAN_TAG_ON    0x0001U
#define CSS1_OUTMODE_STYLE_OPT_ON   0x0002U
#define CSS1_OUTMODE_RULE_ON        0x0003U
#define CSS1_OUTMODE_SPAN_TAG1_ON   0x0004U
#define CSS1_OUTMODE_ANY_ON         0x0007U

// bit 3,4,5
#define CSS1_OUTMODE_SPAN_NO_OFF    0x0000U
#define CSS1_OUTMODE_SPAN_TAG_OFF   (sal_uInt16(0x0001U << 3))
#define CSS1_OUTMODE_STYLE_OPT_OFF  (sal_uInt16(0x0002U << 3))
#define CSS1_OUTMODE_RULE_OFF       (sal_uInt16(0x0003U << 3))
#define CSS1_OUTMODE_ANY_OFF        (sal_uInt16(0x0007U << 3))

#define CSS1_OUTMODE_ONOFF(a) (CSS1_OUTMODE_##a##_ON|CSS1_OUTMODE_##a##_OFF)
#define CSS1_OUTMODE_SPAN_TAG       CSS1_OUTMODE_ONOFF(SPAN_TAG)
#define CSS1_OUTMODE_STYLE_OPT      CSS1_OUTMODE_ONOFF(STYLE_OPT)
#define CSS1_OUTMODE_RULE           CSS1_OUTMODE_ONOFF(RULE)

// the following flags specify what should be outputted
// bit 6,7,8,9
#define CSS1_OUTMODE_TEMPLATE       0x0000U
#define CSS1_OUTMODE_BODY           (sal_uInt16(0x0001U << 6))
#define CSS1_OUTMODE_PARA           (sal_uInt16(0x0002U << 6))
#define CSS1_OUTMODE_HINT           (sal_uInt16(0x0003U << 6))
#define CSS1_OUTMODE_FRAME          (sal_uInt16(0x0004U << 6))
#define CSS1_OUTMODE_TABLE          (sal_uInt16(0x0005U << 6))
#define CSS1_OUTMODE_TABLEBOX       (sal_uInt16(0x0006U << 6))
#define CSS1_OUTMODE_DROPCAP        (sal_uInt16(0x0007U << 6))
#define CSS1_OUTMODE_SECTION        (sal_uInt16(0x0008U << 6))
#define CSS1_OUTMODE_SOURCE         (sal_uInt16(0x000fU << 6))

// bit 10
#define CSS1_OUTMODE_ENCODE         (sal_uInt16(0x0001U << 10))

// bit 11,12,13
// don't care about script
#define CSS1_OUTMODE_ANY_SCRIPT     0x0000U
// no cjk or ctl items
#define CSS1_OUTMODE_WESTERN        (sal_uInt16(0x0001U << 11))
// no western or ctl items
#define CSS1_OUTMODE_CJK            (sal_uInt16(0x0002U << 11))
// no western or cjk items
#define CSS1_OUTMODE_CTL            (sal_uInt16(0x0003U << 11))
// no western, cjk or ctl items
#define CSS1_OUTMODE_NO_SCRIPT      (sal_uInt16(0x0004U << 11))
#define CSS1_OUTMODE_SCRIPT         (sal_uInt16(0x0007U << 11))

// the HTML writer
struct HTMLControl
{
    // the form to which the control belongs
    css::uno::Reference<css::container::XIndexContainer> xFormComps;
    SwNodeOffset nNdIdx;           // the node in which it's anchored
    sal_Int32 nCount;              // how many controls are on the node

    HTMLControl( css::uno::Reference<css::container::XIndexContainer> xForm, SwNodeOffset nIdx );
    ~HTMLControl();

    // operators for the sort array
    bool operator<( const HTMLControl& rCtrl ) const
    {
        return nNdIdx < rCtrl.nNdIdx;
    }
};

class HTMLControls : public o3tl::sorted_vector<std::unique_ptr<HTMLControl>, o3tl::less_ptr_to > {
};

struct SwHTMLFormatInfo
{
    const SwFormat *pFormat;      // the format itself

    OString aToken;             // the token to output
    OUString aClass;            // the class to output

    std::optional<SfxItemSet> moItemSet;   // the attribute set to output

    sal_Int32 nLeftMargin;      // some default values for
    sal_Int32 nRightMargin;     // paragraph styles
    short nFirstLineIndent;

    sal_uInt16 nTopMargin;
    sal_uInt16 nBottomMargin;

    bool bScriptDependent;

    // ctor for a dummy to search
    explicit SwHTMLFormatInfo( const SwFormat *pF ) :
        pFormat( pF ),
        nLeftMargin( 0 ),
        nRightMargin( 0 ),
        nFirstLineIndent(0),
        nTopMargin( 0 ),
        nBottomMargin( 0 ),
        bScriptDependent(false)
    {}

    // ctor for creating of the format information
    SwHTMLFormatInfo( const SwFormat *pFormat, SwDoc *pDoc, SwDoc *pTemplate,
                   bool bOutStyles, LanguageType eDfltLang=LANGUAGE_DONTKNOW,
                   sal_uInt16 nScript=CSS1_OUTMODE_ANY_SCRIPT );
    ~SwHTMLFormatInfo();

    friend bool operator<( const SwHTMLFormatInfo& rInfo1,
                            const SwHTMLFormatInfo& rInfo2 )
    {
        return reinterpret_cast<sal_IntPtr>(rInfo1.pFormat) < reinterpret_cast<sal_IntPtr>(rInfo2.pFormat);
    }

};

typedef std::set<std::unique_ptr<SwHTMLFormatInfo>,
        comphelper::UniquePtrValueLess<SwHTMLFormatInfo>> SwHTMLFormatInfos;

class IDocumentStylePoolAccess;

namespace sw
{
enum class Css1Background
{
    Attr = 1,
    Page = 2,
    Table = 3,
    Fly = 4,
    Section = 5,
    TableRow = 6,
    TableCell = 7,
};
}

class SwHTMLWriter : public Writer
{
    /// XML namespace, in case of XHTML.
    OString maNamespace;

    SwHTMLPosFlyFrames m_aHTMLPosFlyFrames;
    std::unique_ptr<SwHTMLNumRuleInfo> m_pNumRuleInfo;// current numbering
    std::unique_ptr<SwHTMLNumRuleInfo> m_pNextNumRuleInfo;
    sal_uInt32 m_nHTMLMode;               // description of export configuration

    FieldUnit m_eCSS1Unit;

    bool m_bPrettyPrint = true; // Allows to add new lines to make it more readable
    bool m_bLFPossible = false; // a line break can be inserted
    bool m_bSpacePreserve = false; // Using xml::space="preserve", or "white-space: pre-wrap" style
    bool m_bPreserveSpacesOnWrite = false; // If export should use m_bSpacePreserve
    bool m_bInlineHeading = false; // If export should use display:inline for inline heading
    // If "Save URLs relative to *" is ignored for self-generated images / objects
    bool m_bRelativeURLsForOwnObjects = false;

    sal_uInt16 OutHeaderAttrs();
    const SwPageDesc *MakeHeader( sal_uInt16& rHeaderAtrs );
    void GetControls();

    void AddLinkTarget( std::u16string_view aURL );
    void CollectLinkTargets();

    void SetupFilterOptions(std::u16string_view rFilterOptions);

protected:
    ErrCode WriteStream() override;
    void SetupFilterOptions(SfxMedium& rMedium) override;
    void SetupFilterFromPropertyValues(
        const css::uno::Sequence<css::beans::PropertyValue>& rPropertyValues);

public:
    std::vector<OUString> m_aImgMapNames;   // written image maps
    std::set<OUString> m_aImplicitMarks;    // implicit jump marks
    std::set<UIName> m_aNumRuleNames;     // names of exported num rules
    std::set<OUString> m_aScriptParaStyles; // script dependent para styles
    std::set<UIName> m_aScriptTextStyles; // script dependent text styles
    std::vector<OUString> m_aOutlineMarks;
    std::vector<SwNodeOffset> m_aOutlineMarkPoss;
    HTMLControls m_aHTMLControls;     // the forms to be written
    SwHTMLFormatInfos m_CharFormatInfos;
    SwHTMLFormatInfos m_TextCollInfos;
    std::vector<SwFormatINetFormat*> m_aINetFormats; // the "open" INet attributes
    std::optional<std::vector<SwTextFootnote*>> m_xFootEndNotes;

    OUString m_aCSS1Selector;           // style selector
    OUString m_aBulletGrfs[MAXLEVEL];   // list graphics

    css::uno::Reference<css::container::XIndexContainer> mxFormComps; // current form

    rtl::Reference<SwDoc> m_xTemplate;               // HTML template
    std::optional<Color> m_xDfltColor;              // default colour
    SwNodeIndex *m_pStartNdIdx;       // index of first paragraph
    const SwPageDesc *m_pCurrPageDesc;// current page style
    const SwFormatFootnote *m_pFormatFootnote;

    sal_uInt32 m_aFontHeights[7];         // font heights 1-7

    ErrCode m_nWarn;                      // warning code
    sal_uInt64 m_nLastLFPos;              // last position of LF

    HtmlTokenId m_nLastParaToken;         // to hold paragraphs together
    sal_Int32 m_nBkmkTabPos;              // current position in bookmark table
    sal_uInt16 m_nImgMapCnt;
    sal_uInt16 m_nFormCntrlCnt;
    sal_uInt16 m_nEndNote;
    sal_uInt16 m_nFootNote;
    sal_Int32 m_nLeftMargin;              // left indent (e.g. from lists)
    sal_Int32 m_nDfltLeftMargin;          // defaults which doesn't have to be
    sal_Int32 m_nDfltRightMargin;         // written (from template)
    short  m_nFirstLineIndent;            // first line indent (from lists)
    short  m_nDfltFirstLineIndent;        // not to write default
    sal_uInt16 m_nDfltTopMargin;          // defaults which doesn't have to be
    sal_uInt16 m_nDfltBottomMargin;       // written (from template)
    sal_uInt16 m_nIndentLvl;              // How far is it indented?
    sal_Int32 m_nWishLineLen;             // How long can a line be?
    sal_uInt16 m_nDefListLvl;             // which DL level exists now
    sal_Int32  m_nDefListMargin;          // How far is the indentation in DL
    sal_uInt16 m_nHeaderFooterSpace;
    sal_uInt16 m_nTextAttrsToIgnore;
    sal_uInt16 m_nExportMode;
    sal_uInt16 m_nCSS1OutMode;
    sal_uInt16 m_nCSS1Script;         // contains default script (that's the one
                                      // that is not contained in class names)
    SvxFrameDirection   m_nDirection;     // the current direction

    LanguageType        m_eLang;

    // description of the export configuration
    // 0
    bool m_bCfgOutStyles : 1;         // export styles
    bool m_bCfgPreferStyles : 1;      // prefer styles instead of usual tags
    bool m_bCfgFormFeed : 1;          // export form feeds
    bool m_bCfgStarBasic : 1;         // export StarBasic
    bool m_bCfgCpyLinkedGrfs : 1;

    // description of what will be exported

    bool m_bFirstLine : 1;            // is the first line outputted?
    bool m_bTagOn : 1;                // tag on or off i.e. Attr-Start or Attr-End

    // The following two flags specify how attributes are exported:
    // bTextAttr bOutOpts
    // 0        0           style sheets
    // 1        0           Hints: Every attribute will be written as its own tag
    //                             and an end tag exists
    // 0        1           (paragraph)attribute: The Attribute will be exported as option
    //                             of an already written tag. There is no end tag.
    bool m_bTextAttr : 1;
    // 8
    bool m_bOutOpts : 1;

    bool m_bOutTable : 1;             // Will the table content be written?
    bool m_bOutHeader : 1;
    bool m_bOutFooter : 1;
    bool m_bOutFlyFrame : 1;

    // flags for style export

    bool m_bFirstCSS1Rule : 1;        // was a property already written
    bool m_bFirstCSS1Property : 1;    // was a property already written

    // 16
    bool m_bCSS1IgnoreFirstPageDesc : 1;

    // what must/can/may not be written?

    bool m_bNoAlign : 1;              // HTML tag doesn't allow ALIGN=...
    bool m_bClearLeft : 1;            // <BR CLEAR=LEFT> write at end of paragraph
    bool m_bClearRight : 1;           // <BR CLEAR=RIGHT> write at end of paragraph

    // others

    bool m_bPreserveForm : 1;         // preserve the current form

    bool m_bCfgNetscape4 : 1;         // Netscape4 hacks

    bool mbSkipImages : 1;
    /// If HTML header and footer should be written as well, or just the content itself.
    bool mbSkipHeaderFooter : 1;
    bool mbEmbedImages : 1;
    /// Temporary base URL for paste of images.
    std::unique_ptr<utl::TempFileNamed> mpTempBaseURL;
    /// If XHTML markup should be written instead of HTML.
    bool mbXHTML = false;
    /// If the ReqIF subset of XHTML should be written.
    bool mbReqIF = false;

#define sCSS2_P_CLASS_leaders "leaders"
    bool m_bCfgPrintLayout : 1;       // PrintLayout option for TOC dot leaders
    bool m_bParaDotLeaders : 1;       // for TOC dot leaders
    // 26

    /// Tracks which text portion attributes are currently open: a which id -> open count map.
    std::map<sal_uInt16, int> maStartedAttributes;

    OUString m_aRTFOLEMimeType;

    /// ReqIF mode: export images as OLE objects.
    bool m_bExportImagesAsOLE = false;

    /// ReqIF mode: export formulas as PDFs.
    bool m_bExportFormulasAsPDF = false;

    /// DPI used when exporting a vector shape as a bitmap.
    std::optional<sal_Int32> m_nShapeDPI;

    /// If set, replace leading tabs with this many non-breaking spaces.
    std::optional<sal_Int32> m_nLeadingTabWidth;

    /// Construct an instance of SwHTMLWriter and optionally give it
    /// the filter options directly, which can also be set via SetupFilterOptions().
    SW_DLLPUBLIC explicit SwHTMLWriter( const OUString& rBaseURL, std::u16string_view rFilterOptions = std::u16string_view() );
    virtual ~SwHTMLWriter() override;

    void Out_SwDoc( SwPaM* );       // write the marked range

    // output all bookmarks of current paragraph
    void OutAnchor( const OUString& rName );
    void OutBookmarks();
    void OutPointFieldmarks( const SwPosition& rPos );
    void OutImplicitMark( std::u16string_view rMark, const char *pMarkType );

    OUString convertHyperlinkHRefValue(const OUString& rURL);

    void OutHyperlinkHRefValue( const OUString& rURL );

    // output the FlyFrame anchored at current position
    bool OutFlyFrame( SwNodeOffset nNdIdx, sal_Int32 nContentIdx,
                      HtmlPosition nPos );
    void OutFrameFormat( AllHtmlFlags nType, const SwFrameFormat& rFormat,
                         const SdrObject *pSdrObj );

    void OutForm( bool bTagOn=true, const SwStartNode *pStNd=nullptr );
    void OutHiddenForms();
    void OutHiddenForm( const css::uno::Reference<css::form::XForm>& rForm );

    void OutForm( bool bOn, const css::uno::Reference<css::container::XIndexContainer>& rFormComps );
    void OutHiddenControls( const css::uno::Reference<css::container::XIndexContainer>& rFormComps,
                            const css::uno::Reference<css::beans::XPropertySet>& rPropSet );
    bool HasControls() const;

    void OutFootEndNoteInfo();
    void OutFootEndNotes();
    OUString GetFootEndNoteSym( const SwFormatFootnote& rFormatFootnote );
    void OutFootEndNoteSym( const SwFormatFootnote& rFormatFootnote, std::u16string_view rNum,
                             sal_uInt16 nScript );

    void OutBasic(const SwHTMLWriter& rHTMLWrt);

    // Used to indent inner blocks using dl/dd tags
    void OutAndSetDefList( sal_uInt16 nNewLvl );

    void OutStyleSheet( const SwPageDesc& rPageDesc );

    inline void OutCSS1_PropertyAscii( std::string_view pProp,
                                       std::string_view rVal );
    inline void OutCSS1_Property( std::string_view pProp, const OUString& rVal );
    void OutCSS1_Property( std::string_view pProp, std::string_view pVal,
                           const OUString *pSVal, std::optional<sw::Css1Background> oBackground = std::nullopt );
    void OutCSS1_UnitProperty( std::string_view pProp, tools::Long nVal );
    void OutCSS1_PixelProperty( std::string_view pProp, tools::Long nTwips );
    void OutCSS1_SfxItemSet( const SfxItemSet& rItemSet, bool bDeep=true, std::string_view rAdd = {} );

    // events of BODY tag from SFX configuration
    void OutBasicBodyEvents();

    // BACKGROUND/BGCOLOR option
    void OutBackground( const SvxBrushItem *pBrushItem, bool bGraphic );
    void OutBackground( const SfxItemSet& rItemSet, bool bGraphic );

    void OutLanguage( LanguageType eLang );
    SvxFrameDirection GetHTMLDirection( SvxFrameDirection nDir ) const;
    SvxFrameDirection GetHTMLDirection( const SfxItemSet& rItemSet ) const;
    void OutDirection( SvxFrameDirection nDir );
    static OString convertDirection(SvxFrameDirection nDirection);

    // ALT/ALIGN/WIDTH/HEIGHT/HSPACE/VSPACE option of current
    // frame format output and maybe add a <BR CLEAR=...> at the
    // beginning of rEndTags
    OString OutFrameFormatOptions( const SwFrameFormat& rFrameFormat, std::u16string_view rAltText,
                                   HtmlFrmOpts nFrameOpts );

    void writeFrameFormatOptions(HtmlWriter& aHtml, const SwFrameFormat& rFrameFormat, const OUString& rAltText, HtmlFrmOpts nFrameOpts);

    void writeFrameSurroundTag(HtmlWriter& aHtml, const SwFrameFormat& rFrameFormat, HtmlFrmOpts nFrameOpts);

    /// Writes the formatting for tables.
    void OutCSS1_TableFrameFormatOptions( const SwFrameFormat& rFrameFormat );

    /// Writes the borders and background for table cells.
    void OutCSS1_TableCellBordersAndBG(const SwFrameFormat& rFrameFormat, const SvxBrushItem *pBrushItem);

    void OutCSS1_SectionFormatOptions( const SwFrameFormat& rFrameFormat, const SwFormatCol *pCol );
    void OutCSS1_FrameFormatOptions( const SwFrameFormat& rFrameFormat, HtmlFrmOpts nFrameOpts,
                                     const SdrObject *pSdrObj=nullptr,
                                     const SfxItemSet *pItemSet=nullptr );
    void OutCSS1_FrameFormatBackground( const SwFrameFormat& rFrameFormat );

    void ChangeParaToken( HtmlTokenId nNew );

    void IncIndentLevel()
    {
        m_nIndentLvl++;
    }
    void DecIndentLevel()
    {
        if ( m_nIndentLvl ) m_nIndentLvl--;
    }
    OString GetIndentString(sal_uInt16 nIncLvl = 0);

    sal_Int32 GetLineLen()
    {
        return static_cast<sal_Int32>(Strm().Tell()-m_nLastLFPos);
    }
    void OutNewLine( bool bCheck=false );

    // for HTMLSaveData
    SwPaM* GetEndPaM() { return m_pOrigPam; }
    void SetEndPaM( SwPaM* pPam ) { m_pOrigPam = pPam; }

    static sal_uInt32 ToPixel(sal_uInt32 nTwips);
    static Size ToPixel(Size aTwips);

    SwHTMLFrameType GuessFrameType( const SwFrameFormat& rFrameFormat,
                         const SdrObject*& rpStrObj );
    static SwHTMLFrameType GuessOLENodeFrameType( const SwNode& rNd );

    void CollectFlyFrames();

    sal_uInt16 GetHTMLFontSize( sal_uInt32 nFontHeight ) const;

    // Fetch current numbering information.
    SwHTMLNumRuleInfo& GetNumInfo() { return *m_pNumRuleInfo; }

    // Fetch current numbering information of next paragraph. They
    // don't have to exist yet!
    SwHTMLNumRuleInfo *GetNextNumInfo() { return m_pNextNumRuleInfo.get(); }
    std::unique_ptr<SwHTMLNumRuleInfo> ReleaseNextNumInfo();

    // Set the numbering information of next paragraph.
    void SetNextNumInfo( std::unique_ptr<SwHTMLNumRuleInfo> pNxt );

    // Fill the numbering information of next paragraph.
    void FillNextNumInfo();

    // Clear numbering information of next paragraph.
    void ClearNextNumInfo();

    static const SdrObject* GetHTMLControl( const SwDrawFrameFormat& rFormat );
    static const SdrObject* GetMarqueeTextObj( const SwDrawFrameFormat& rFormat );
    static sal_uInt16 GetCSS1Selector( const SwFormat *pFormat, OString& rToken,
                                   OUString& rClass, sal_uInt16& rRefPoolId,
                                   OUString *pPseudo=nullptr );

    static const SwFormat *GetTemplateFormat( sal_uInt16 nPoolId, IDocumentStylePoolAccess* /*SwDoc*/ pTemplate );
    static const SwFormat *GetParentFormat( const SwFormat& rFormat, sal_uInt16 nDeep );

    static void SubtractItemSet( SfxItemSet& rItemSet,
                                 const SfxItemSet& rRefItemSet,
                                 bool bSetDefaults,
                                 bool bClearSame = true,
                                   const SfxItemSet *pRefScriptItemSet=nullptr );
    static bool HasScriptDependentItems( const SfxItemSet& rItemSet,
                                               bool bCheckDropCap );

    static void GetEEAttrsFromDrwObj( SfxItemSet& rItemSet,
                                      const SdrObject *pObj );

    static sal_uInt16 GetDefListLvl( const UIName& rNm, sal_uInt16 nPoolId );

    sal_uInt32 GetHTMLMode() const
    {
        return m_nHTMLMode;
    }
    bool IsHTMLMode( sal_uInt32 nMode ) const
    {
        return (m_nHTMLMode & nMode) != 0;
    }

    inline bool IsCSS1Source( sal_uInt16 n ) const;
    inline bool IsCSS1Script( sal_uInt16 n ) const;

    static const char *GetNumFormat( sal_uInt16 nFormat );
    static void PrepareFontList( const SvxFontItem& rFontItem, OUString& rNames,
                                 sal_Unicode cQuote, bool bGeneric );
    static sal_uInt16 GetCSS1ScriptForScriptType( sal_uInt16 nScriptType );
    static TypedWhichId<SvxLanguageItem> GetLangWhichIdFromScript( sal_uInt16 nScript );

    FieldUnit GetCSS1Unit() const { return m_eCSS1Unit; }

    sal_Int32 indexOfDotLeaders( sal_uInt16 nPoolId, std::u16string_view rText );

    /// Determines the prefix string needed to respect the requested namespace alias.
    OString GetNamespace() const { return maNamespace; }

    bool IsPrettyPrint() const { return !m_bSpacePreserve && m_bPrettyPrint; }
    bool IsLFPossible() const { return !m_bSpacePreserve && m_bLFPossible; }
    void SetLFPossible(bool val) { m_bLFPossible = val; }
    bool IsSpacePreserve() const { return m_bSpacePreserve; }
    void SetSpacePreserve(bool val) { m_bSpacePreserve = val; }
    bool IsPreserveSpacesOnWritePrefSet() const { return m_bPreserveSpacesOnWrite; }
    bool IsInlineHeading() const { return m_bInlineHeading; }

    OUString normalizeURL(const OUString& url, bool own) const;
};

inline bool SwHTMLWriter::IsCSS1Source( sal_uInt16 n ) const
{
    return n == (m_nCSS1OutMode & CSS1_OUTMODE_SOURCE);
}

inline bool SwHTMLWriter::IsCSS1Script( sal_uInt16 n ) const
{
    sal_uInt16 nScript = (m_nCSS1OutMode & CSS1_OUTMODE_SCRIPT);
    return CSS1_OUTMODE_ANY_SCRIPT == nScript || n == nScript;
}

inline void SwHTMLWriter::OutCSS1_PropertyAscii( std::string_view pProp,
                                                 std::string_view rVal )
{
    OutCSS1_Property( pProp, rVal, nullptr );
}

inline void SwHTMLWriter::OutCSS1_Property( std::string_view pProp,
                                            const OUString& rVal )
{
    OutCSS1_Property( pProp, std::string_view(), &rVal );
}


// Structure caches the current data of the writer to output
// another part of the document, like e.g. header/footer
// With the two USHORTs in the ctor a new PaM is created and sets the
// positions in the document.
// In dtor all data is restored and the created PaM is deleted again.

class HTMLSaveData
{
    SwHTMLWriter& rWrt;
    std::shared_ptr<SwUnoCursor> pOldPam;
    SwPaM *pOldEnd;
    std::unique_ptr<SwHTMLNumRuleInfo> pOldNumRuleInfo;     // Owner = this
    std::unique_ptr<SwHTMLNumRuleInfo> pOldNextNumRuleInfo;
    sal_uInt16 nOldDefListLvl;
    SvxFrameDirection nOldDirection;
    bool bOldWriteAll : 1;
    bool bOldOutHeader : 1;
    bool bOldOutFooter : 1;
    bool bOldOutFlyFrame : 1;

public:
    HTMLSaveData( SwHTMLWriter&, SwNodeOffset nStt, SwNodeOffset nEnd,
                  bool bSaveNum=true,
                  const SwFrameFormat *pFrameFormat=nullptr  );
    ~HTMLSaveData();
};

// some function prototypes
SwHTMLWriter& OutHTML_FrameFormatOLENode( SwHTMLWriter& rWrt, const SwFrameFormat& rFormat,
                               bool bInCntnr );
SwHTMLWriter& OutHTML_FrameFormatOLENodeGrf( SwHTMLWriter& rWrt, const SwFrameFormat& rFormat,
                                  bool bInCntnr, bool bWriteReplacementGraphic = true );

SwHTMLWriter& OutHTML_SwTextNode( SwHTMLWriter&, const SwContentNode& );
SwHTMLWriter& OutHTML_SwTableNode( SwHTMLWriter& , SwTableNode &, const SwFrameFormat *,
                           const OUString* pCaption=nullptr, bool bTopCaption=false );

SwHTMLWriter& OutHTML_DrawFrameFormatAsControl( SwHTMLWriter& rWrt, const SwDrawFrameFormat& rFormat,
                                     const SdrUnoObj& rSdrObj, bool bInCntnr );
SwHTMLWriter& OutHTML_DrawFrameFormatAsMarquee( SwHTMLWriter& rWrt, const SwDrawFrameFormat& rFormat,
                                     const SdrObject& rSdrObj );

SwHTMLWriter& OutHTML_HeaderFooter( SwHTMLWriter& rWrt, const SwFrameFormat& rFrameFormat,
                              bool bHeader );

SwHTMLWriter& OutHTML_ImageStart( HtmlWriter& rHtml, SwHTMLWriter&, const SwFrameFormat& rFormat,
                       const OUString& rGraphicURL,
                       Graphic const & rGraphic, const OUString& rAlternateText,
                       const Size& rRealSize, HtmlFrmOpts nFrameOpts,
                       const char *pMarkType,
                       const ImageMap *pGenImgMap,
                       const OUString& rMimeType,
                       bool bOwn = false );
SwHTMLWriter& OutHTML_ImageEnd( HtmlWriter& rHtml, SwHTMLWriter& );

SwHTMLWriter& OutHTML_BulletImage( SwHTMLWriter& rWrt, const char *pTag,
                             const SvxBrushItem* pBrush,
                             const OUString& rGraphicURL);

SwHTMLWriter& OutHTML_SwFormatField( SwHTMLWriter& rWrt, const SfxPoolItem& rHt );
SwHTMLWriter& OutHTML_SwFormatFootnote( SwHTMLWriter& rWrt, const SfxPoolItem& rHt );
SwHTMLWriter& OutHTML_SwFormatLineBreak(SwHTMLWriter& rWrt, const SfxPoolItem& rHt);
SwHTMLWriter& OutHTML_INetFormat( SwHTMLWriter&, const SwFormatINetFormat& rINetFormat, bool bOn );

SwHTMLWriter& OutCSS1_BodyTagStyleOpt( SwHTMLWriter& rWrt, const SfxItemSet& rItemSet );
SwHTMLWriter& OutCSS1_ParaTagStyleOpt( SwHTMLWriter& rWrt, const SfxItemSet& rItemSet, std::string_view rAdd = {} );

SwHTMLWriter& OutCSS1_HintSpanTag( SwHTMLWriter& rWrt, const SfxPoolItem& rHt );
SwHTMLWriter& OutCSS1_HintStyleOpt( SwHTMLWriter& rWrt, const SfxPoolItem& rHt );

/// Writes the background of table rows.
SwHTMLWriter& OutCSS1_TableBGStyleOpt( SwHTMLWriter& rWrt, const SfxPoolItem& rHt );

SwHTMLWriter& OutCSS1_NumberBulletListStyleOpt( SwHTMLWriter& rWrt, const SwNumRule& rNumRule,
                                    sal_uInt8 nLevel );

SwHTMLWriter& OutHTML_NumberBulletListStart( SwHTMLWriter& rWrt,
                                 const SwHTMLNumRuleInfo& rInfo );
SwHTMLWriter& OutHTML_NumberBulletListEnd( SwHTMLWriter& rWrt,
                               const SwHTMLNumRuleInfo& rNextInfo );

SwHTMLWriter& OutCSS1_SvxBox( SwHTMLWriter& rWrt, const SfxPoolItem& rHt );

OString GetCSS1_Color(const Color& rColor);

/// Determines if rProperty with a given rValue has to be suppressed due to ReqIF mode.
bool IgnorePropertyForReqIF(bool bReqIF, std::string_view rProperty, std::string_view rValue,
                            std::optional<sw::Css1Background> oBackground = std::nullopt);

#endif // INCLUDED_SW_SOURCE_FILTER_HTML_WRTHTML_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
