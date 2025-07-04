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

#include "SchXMLChartContext.hxx"
#include <SchXMLImport.hxx>
#include "SchXMLLegendContext.hxx"
#include "SchXMLDataTableContext.hxx"
#include "SchXMLPlotAreaContext.hxx"
#include "SchXMLParagraphContext.hxx"
#include "SchXMLTableContext.hxx"
#include "SchXMLSeries2Context.hxx"
#include "SchXMLTools.hxx"
#include <osl/diagnose.h>
#include <sal/log.hxx>
#include <comphelper/diagnose_ex.hxx>
#include <unotools/mediadescriptor.hxx>
#include <utility>
#include <xmloff/xmlnamespace.hxx>
#include <xmloff/xmltoken.hxx>
#include <xmloff/namespacemap.hxx>
#include <xmloff/xmluconv.hxx>
#include <xmloff/xmlstyle.hxx>
#include <xmloff/SchXMLSeriesHelper.hxx>

#include <vector>
#include <com/sun/star/lang/XMultiServiceFactory.hpp>
#include <com/sun/star/chart/XChartDocument.hpp>
#include <com/sun/star/chart/XDiagram.hpp>
#include <com/sun/star/drawing/XDrawPageSupplier.hpp>
#include <com/sun/star/embed/Aspects.hpp>
#include <com/sun/star/embed/XVisualObject.hpp>

#include <com/sun/star/chart2/XChartDocument.hpp>
#include <com/sun/star/chart2/data/XDataSink.hpp>
#include <com/sun/star/chart2/data/XPivotTableDataProvider.hpp>
#include <com/sun/star/chart2/XDataSeriesContainer.hpp>
#include <com/sun/star/chart2/XCoordinateSystemContainer.hpp>
#include <com/sun/star/chart2/XChartTypeContainer.hpp>
#include <com/sun/star/chart2/XTitled.hpp>

#include <com/sun/star/container/XChild.hpp>
#include <com/sun/star/chart2/data/XDataReceiver.hpp>
#include <o3tl/safeint.hxx>
#include <o3tl/string_view.hxx>

using namespace com::sun::star;
using namespace ::xmloff::token;
using com::sun::star::uno::Reference;
using namespace ::SchXMLTools;

namespace
{

void lcl_setRoleAtLabeledSequence(
    const uno::Reference< chart2::data::XLabeledDataSequence > & xLSeq,
    const OUString &rRole )
{
    // set role of sequence
    uno::Reference< chart2::data::XDataSequence > xValues( xLSeq->getValues());
    if( xValues.is())
    {
        uno::Reference< beans::XPropertySet > xProp( xValues, uno::UNO_QUERY );
        if( xProp.is())
            xProp->setPropertyValue(u"Role"_ustr, uno::Any( rRole ));
    }
}

void lcl_MoveDataToCandleStickSeries(
    const uno::Reference< chart2::data::XDataSource > & xDataSource,
    const uno::Reference< chart2::XDataSeries > & xDestination,
    const OUString & rRole )
{
    try
    {
        uno::Sequence< uno::Reference< chart2::data::XLabeledDataSequence > > aLabeledSeq(
            xDataSource->getDataSequences());
        if( aLabeledSeq.hasElements())
        {
            lcl_setRoleAtLabeledSequence( aLabeledSeq[0], rRole );

            // add to data series
            uno::Reference< chart2::data::XDataSource > xSource( xDestination, uno::UNO_QUERY_THROW );
            // @todo: realloc only once outside this function
            uno::Sequence< uno::Reference< chart2::data::XLabeledDataSequence > > aData( xSource->getDataSequences());
            aData.realloc( aData.getLength() + 1);
            aData.getArray()[ aData.getLength() - 1 ] = aLabeledSeq[0];
            uno::Reference< chart2::data::XDataSink > xSink( xDestination, uno::UNO_QUERY_THROW );
            xSink->setData( aData );
        }
    }
    catch(const uno::Exception&)
    {
        TOOLS_WARN_EXCEPTION("xmloff.chart", "Exception caught while moving data to candlestick series" );
    }
}

void lcl_setRoleAtFirstSequence(
    const uno::Reference< chart2::XDataSeries > & xSeries,
    const OUString & rRole )
{
    uno::Reference< chart2::data::XDataSource > xSource( xSeries, uno::UNO_QUERY );
    if( xSource.is())
    {
        uno::Sequence< uno::Reference< chart2::data::XLabeledDataSequence > > aSeq( xSource->getDataSequences());
        if( aSeq.hasElements())
            lcl_setRoleAtLabeledSequence( aSeq[0], rRole );
    }
}

void lcl_removeEmptyChartTypeGroups( const uno::Reference< chart2::XChartDocument > & xDoc )
{
    if( ! xDoc.is())
        return;

    uno::Reference< chart2::XDiagram > xDia( xDoc->getFirstDiagram());
    if( ! xDia.is())
        return;

    try
    {
        // count all charttype groups to be able to leave at least one
        sal_Int32 nRemainingGroups = 0;
        uno::Reference< chart2::XCoordinateSystemContainer > xCooSysCnt( xDia, uno::UNO_QUERY_THROW );
        const uno::Sequence< uno::Reference< chart2::XCoordinateSystem > >
            aCooSysSeq( xCooSysCnt->getCoordinateSystems());
        for( auto const & i : aCooSysSeq )
        {
            uno::Reference< chart2::XChartTypeContainer > xCTCnt( i, uno::UNO_QUERY_THROW );
            nRemainingGroups += xCTCnt->getChartTypes().getLength();
        }

        // delete all empty groups, but leave at least  group (empty or not)
        for( sal_Int32 nI = aCooSysSeq.getLength(); nI-- && (nRemainingGroups > 1); )
        {
            uno::Reference< chart2::XChartTypeContainer > xCTCnt( aCooSysSeq[nI], uno::UNO_QUERY_THROW );
            uno::Sequence< uno::Reference< chart2::XChartType > > aCTSeq( xCTCnt->getChartTypes());
            for( sal_Int32 nJ=aCTSeq.getLength(); nJ-- && (nRemainingGroups > 1); )
            {
                uno::Reference< chart2::XDataSeriesContainer > xDSCnt( aCTSeq[nJ], uno::UNO_QUERY_THROW );
                if( !xDSCnt->getDataSeries().hasElements() )
                {
                    // note: iterator stays valid as we have a local sequence
                    xCTCnt->removeChartType( aCTSeq[nJ] );
                    --nRemainingGroups;
                }
            }
        }
    }
    catch(const uno::Exception&)
    {
        TOOLS_INFO_EXCEPTION("xmloff.chart", "Exception caught while removing empty chart types");
    }
}

uno::Sequence< sal_Int32 > lcl_getNumberSequenceFromString( std::u16string_view rStr, bool bAddOneToEachOldIndex )
{
    const sal_Unicode aSpace( ' ' );

    // count number of entries
    ::std::vector< sal_Int32 > aVec;
    size_t nLastPos = 0;
    size_t nPos = 0;
    while( nPos != std::u16string_view::npos )
    {
        nPos = rStr.find( aSpace, nLastPos );
        if( nPos != std::u16string_view::npos )
        {
            if( nPos > nLastPos )
                aVec.push_back( o3tl::toInt32(rStr.substr( nLastPos, (nPos - nLastPos) )) );
            nLastPos = nPos + 1;
        }
    }
    // last entry
    if( nLastPos != 0 &&
        rStr.size() > nLastPos )
    {
        aVec.push_back( o3tl::toInt32(rStr.substr( nLastPos )) );
    }

    const size_t nVecSize = aVec.size();
    uno::Sequence< sal_Int32 > aSeq( nVecSize );

    if(!bAddOneToEachOldIndex)
    {
        sal_Int32* pSeqArr = aSeq.getArray();
        for( nPos = 0; nPos < nVecSize; ++nPos )
        {
            pSeqArr[ nPos ] = aVec[ nPos ];
        }
    }
    else if( bAddOneToEachOldIndex )
    {
        aSeq.realloc( nVecSize+1 );
        auto pSeqArr = aSeq.getArray();
        pSeqArr[0]=0;

        for( nPos = 0; nPos < nVecSize; ++nPos )
        {
            pSeqArr[ nPos+1 ] = aVec[ nPos ]+1;
        }
    }

    return aSeq;
}

} // anonymous namespace

SchXMLChartContext::SchXMLChartContext( SchXMLImportHelper& rImpHelper,
                                        SvXMLImport& rImport ) :
        SvXMLImportContext( rImport ),
        mrImportHelper( rImpHelper ),
        m_bHasRangeAtPlotArea( false ),
        m_bHasTableElement( false ),
        mbAllRangeAddressesAvailable( true ),
        mbColHasLabels( false ),
        mbRowHasLabels( false ),
        meDataRowSource( chart::ChartDataRowSource_COLUMNS ),
        mbIsStockChart( false ),
        mPieSubType(css::chart2::PieChartSubType_NONE),
        mfPieSplitPos(2.0)
{
}

SchXMLChartContext::~SchXMLChartContext()
{}

static bool lcl_hasServiceName(Reference<lang::XMultiServiceFactory> const & xFactory, OUString const & rServiceName)
{
    const uno::Sequence<OUString> aServiceNames(xFactory->getAvailableServiceNames());

    return std::find(aServiceNames.begin(), aServiceNames.end(), rServiceName) != aServiceNames.end();
}

void setDataProvider(uno::Reference<chart2::XChartDocument> const & xChartDoc, OUString const & sDataPilotSource)
{
    if (!xChartDoc.is())
        return;

    try
    {
        uno::Reference<container::XChild> xChild(xChartDoc, uno::UNO_QUERY);
        uno::Reference<chart2::data::XDataReceiver> xDataReceiver(xChartDoc, uno::UNO_QUERY);
        if (xChild.is() && xDataReceiver.is())
        {
            bool bHasOwnData = true;

            Reference<lang::XMultiServiceFactory> xFact(xChild->getParent(), uno::UNO_QUERY);
            if (xFact.is())
            {
                if (!xChartDoc->getDataProvider().is())
                {
                    bool bHasDataPilotSource = !sDataPilotSource.isEmpty();
                    OUString aDataProviderServiceName(u"com.sun.star.chart2.data.DataProvider"_ustr);
                    if (bHasDataPilotSource)
                        aDataProviderServiceName = "com.sun.star.chart2.data.PivotTableDataProvider";

                    if (lcl_hasServiceName(xFact, aDataProviderServiceName))
                    {
                        Reference<chart2::data::XDataProvider> xProvider(xFact->createInstance(aDataProviderServiceName), uno::UNO_QUERY);

                        if (xProvider.is())
                        {
                            if (bHasDataPilotSource)
                            {
                                Reference<chart2::data::XPivotTableDataProvider> xPivotTableDataProvider(xProvider, uno::UNO_QUERY);
                                xPivotTableDataProvider->setPivotTableName(sDataPilotSource);
                                xDataReceiver->attachDataProvider(xProvider);
                                bHasOwnData = !xPivotTableDataProvider->hasPivotTable();
                            }
                            else
                            {
                                xDataReceiver->attachDataProvider(xProvider);
                                bHasOwnData = false;
                            }
                        }
                    }
                }
                else
                    bHasOwnData = false;
            }
            // else we have no parent => we have our own data

            if (bHasOwnData && ! xChartDoc->hasInternalDataProvider())
                xChartDoc->createInternalDataProvider(false);
        }
    }
    catch (const uno::Exception &)
    {
        TOOLS_INFO_EXCEPTION("xmloff.chart", "SchXMLChartContext::StartElement()");
    }
}

void SchXMLChartContext::startFastElement( sal_Int32 /*nElement*/,
    const css::uno::Reference< css::xml::sax::XFastAttributeList >& xAttrList )
{
    // parse attributes

    uno::Reference< embed::XVisualObject > xVisualObject( mrImportHelper.GetChartDocument(), uno::UNO_QUERY);
    SAL_WARN_IF(!xVisualObject.is(), "xmloff.chart", "need xVisualObject for page size");
    if( xVisualObject.is() )
        maChartSize = xVisualObject->getVisualAreaSize( embed::Aspects::MSOLE_CONTENT ); //#i103460# take the size given from the parent frame as default

    OUString sAutoStyleName;
    OUString aOldChartTypeName;
    bool bHasAddin = false;
    mPieSubType = css::chart2::PieChartSubType_NONE;

    for( auto& aIter : sax_fastparser::castToFastAttributeList(xAttrList) )
    {
        switch( aIter.getToken() )
        {
            case XML_ELEMENT(LO_EXT, XML_DATA_PILOT_SOURCE):
                msDataPilotSource = aIter.toString();
                break;
            case XML_ELEMENT( XLINK, XML_TYPE ):
                // Ignored for now.
            break;
            case XML_ELEMENT(XLINK, XML_HREF):
                m_aXLinkHRefAttributeToIndicateDataProvider = aIter.toString();
                break;
            case XML_ELEMENT(CHART, XML_CLASS):
                {
                    OUString aValue = aIter.toString();
                    OUString sClassName;
                    sal_uInt16 nClassPrefix =
                        GetImport().GetNamespaceMap().GetKeyByAttrValueQName(
                                aValue, &sClassName );
                    if( XML_NAMESPACE_CHART == nClassPrefix )
                    {
                        SchXMLChartTypeEnum eChartTypeEnum = SchXMLTools::GetChartTypeEnum( sClassName );
                        if( eChartTypeEnum != XML_CHART_CLASS_UNKNOWN )
                        {
                            aOldChartTypeName = SchXMLTools::GetChartTypeByClassName( sClassName, true /* bUseOldNames */ );
                            maChartTypeServiceName = SchXMLTools::GetChartTypeByClassName( sClassName, false /* bUseOldNames */ );
                            switch( eChartTypeEnum )
                            {
                            case XML_CHART_CLASS_STOCK:
                                mbIsStockChart = true;
                                break;
                            default:
                                break;
                            }
                        }
                    }
                    else if( XML_NAMESPACE_OOO == nClassPrefix )
                    {
                        // service is taken from add-in-name attribute
                        bHasAddin = true;

                        aOldChartTypeName = sClassName;
                        maChartTypeServiceName = sClassName;
                    }
                }
                break;

            case XML_ELEMENT(SVG, XML_WIDTH):
            case XML_ELEMENT(SVG_COMPAT, XML_WIDTH):
                GetImport().GetMM100UnitConverter().convertMeasureToCore(
                        maChartSize.Width, aIter.toView() );
                break;

            case XML_ELEMENT(SVG, XML_HEIGHT):
            case XML_ELEMENT(SVG_COMPAT, XML_HEIGHT):
                GetImport().GetMM100UnitConverter().convertMeasureToCore(
                        maChartSize.Height, aIter.toView() );
                break;

            case XML_ELEMENT(CHART, XML_STYLE_NAME):
                sAutoStyleName = aIter.toString();
                break;

            case XML_ELEMENT(CHART, XML_COLUMN_MAPPING):
                msColTrans = aIter.toString();
                break;
            case XML_ELEMENT(CHART,  XML_ROW_MAPPING):
                msRowTrans = aIter.toString();
                break;
            case XML_ELEMENT(LO_EXT, XML_SUB_BAR):
                if (aIter.toString().toBoolean()) {
                    mPieSubType = css::chart2::PieChartSubType_BAR;
                }
                break;
            case XML_ELEMENT(LO_EXT, XML_SUB_PIE):
                if (aIter.toString().toBoolean()) {
                    mPieSubType = css::chart2::PieChartSubType_PIE;
                }
                break;
            case XML_ELEMENT(LO_EXT, XML_SPLIT_POSITION):
                mfPieSplitPos = aIter.toDouble();
                break;
            default:
                XMLOFF_WARN_UNKNOWN("xmloff", aIter);
        }
    }

    uno::Reference<chart::XChartDocument> xDoc = mrImportHelper.GetChartDocument();
    uno::Reference<chart2::XChartDocument> xNewDoc(xDoc, uno::UNO_QUERY);

    setDataProvider(xNewDoc, msDataPilotSource);

    if( aOldChartTypeName.isEmpty() )
    {
        SAL_WARN("xmloff.chart", "need a charttype to create a diagram" );
        //set a fallback value:
        const OUString& aChartClass_Bar( GetXMLToken(XML_BAR ) );
        aOldChartTypeName = SchXMLTools::GetChartTypeByClassName( aChartClass_Bar, true /* bUseOldNames */ );
        maChartTypeServiceName = SchXMLTools::GetChartTypeByClassName( aChartClass_Bar, false /* bUseOldNames */ );
    }

    //  Set the size of the draw page.
    if( xVisualObject.is() )
        xVisualObject->setVisualAreaSize( embed::Aspects::MSOLE_CONTENT, maChartSize );

    InitChart( aOldChartTypeName);

    if( bHasAddin )
    {
        //correct charttype service name when having an addin
        //and don't refresh addin during load
        uno::Reference< beans::XPropertySet > xDocProp( mrImportHelper.GetChartDocument(), uno::UNO_QUERY );
        if( xDocProp.is() )
        {
            try
            {
                xDocProp->getPropertyValue(u"BaseDiagram"_ustr) >>= aOldChartTypeName;
                maChartTypeServiceName =  SchXMLTools::GetNewChartTypeName( aOldChartTypeName );
                xDocProp->setPropertyValue(u"RefreshAddInAllowed"_ustr, uno::Any( false) );
            }
            catch(const uno::Exception&)
            {
                TOOLS_WARN_EXCEPTION("xmloff.chart", "Exception during import SchXMLChartContext::StartElement" );
            }
        }
    }

    // set auto-styles for Area
    uno::Reference<beans::XPropertySet> xProp = mrImportHelper.GetChartDocument()->getArea();
    mrImportHelper.FillAutoStyle(sAutoStyleName, xProp);
}

namespace
{

struct NewDonutSeries
{
    css::uno::Reference< css::chart2::XDataSeries > m_xSeries;
    OUString msStyleName;
    sal_Int32 mnAttachedAxis;

    ::std::vector< OUString > m_aSeriesStyles;
    ::std::vector< OUString > m_aPointStyles;

    NewDonutSeries( css::uno::Reference< css::chart2::XDataSeries > xSeries, sal_Int32 nPointCount )
                    : m_xSeries(std::move( xSeries ))
                    , mnAttachedAxis( 1 )
    {
        m_aPointStyles.resize(nPointCount);
        m_aSeriesStyles.resize(nPointCount);
    }

    void setSeriesStyleNameToPoint( const OUString& rStyleName, sal_Int32 nPointIndex )
    {
        SAL_WARN_IF(nPointIndex >= static_cast<sal_Int32>(m_aSeriesStyles.size()), "xmloff.chart", "donut point <-> series count mismatch");
        if( nPointIndex < static_cast<sal_Int32>(m_aSeriesStyles.size()) )
            m_aSeriesStyles[nPointIndex]=rStyleName;
    }

    void setPointStyleNameToPoint( const OUString& rStyleName, sal_Int32 nPointIndex )
    {
        SAL_WARN_IF(nPointIndex >= static_cast<sal_Int32>(m_aPointStyles.size()), "xmloff.chart", "donut point <-> series count mismatch");
        if( nPointIndex < static_cast<sal_Int32>(m_aPointStyles.size()) )
            m_aPointStyles[nPointIndex]=rStyleName;
    }

    ::std::vector< DataRowPointStyle > creatStyleVector()
    {
        ::std::vector< DataRowPointStyle > aRet;

        DataRowPointStyle aSeriesStyle( DataRowPointStyle::DATA_SERIES
            , m_xSeries, -1, 1, msStyleName, mnAttachedAxis );
        aRet.push_back( aSeriesStyle );

        sal_Int32 nPointIndex=0;
        for (auto const& pointStyle : m_aPointStyles)
        {
            DataRowPointStyle aPointStyle( DataRowPointStyle::DATA_POINT
                , m_xSeries, nPointIndex, 1, pointStyle, mnAttachedAxis );
            if( nPointIndex < static_cast<sal_Int32>(m_aSeriesStyles.size()) )
            {
                aPointStyle.msSeriesStyleNameForDonuts = m_aSeriesStyles[nPointIndex];
            }
            if( !aPointStyle.msSeriesStyleNameForDonuts.isEmpty()
                || !aPointStyle.msStyleName.isEmpty() )
                aRet.push_back( aPointStyle );
            ++nPointIndex;
        }

        return aRet;
    }
};

void lcl_swapPointAndSeriesStylesForDonutCharts( ::std::vector< DataRowPointStyle >& rStyleVector
        , ::std::map< css::uno::Reference< css::chart2::XDataSeries> , sal_Int32 >&& aSeriesMap )
{
    //detect old series count
    //and add old series to aSeriesMap
    sal_Int32 nOldSeriesCount = 0;
    {
        sal_Int32 nMaxOldSeriesIndex = 0;
        sal_Int32 nOldSeriesIndex = 0;
        for (auto const& style : rStyleVector)
        {
            DataRowPointStyle aStyle(style);
            if(aStyle.meType == DataRowPointStyle::DATA_SERIES &&
                    aStyle.m_xSeries.is() )
            {
                nMaxOldSeriesIndex = nOldSeriesIndex;

                if( aSeriesMap.end() == aSeriesMap.find(aStyle.m_xSeries) )
                    aSeriesMap[aStyle.m_xSeries] = nOldSeriesIndex;

                nOldSeriesIndex++;
            }
        }
        nOldSeriesCount = nMaxOldSeriesIndex+1;
    }

    //initialize new series styles
    ::std::map< Reference< chart2::XDataSeries >, sal_Int32 >::const_iterator aSeriesMapEnd( aSeriesMap.end() );

    //sort by index
    ::std::vector< NewDonutSeries > aNewSeriesVector;
    {
        ::std::map< sal_Int32, Reference< chart2::XDataSeries > > aIndexSeriesMap;
        for (auto const& series : aSeriesMap)
            aIndexSeriesMap[series.second] = series.first;

        for (auto const& indexSeries : aIndexSeriesMap)
            aNewSeriesVector.emplace_back(indexSeries.second,nOldSeriesCount );
    }

    //overwrite attached axis information according to old series styles
    for (auto const& style : rStyleVector)
    {
        DataRowPointStyle aStyle(style);
        if(aStyle.meType == DataRowPointStyle::DATA_SERIES )
        {
            auto aSeriesMapIt = aSeriesMap.find( aStyle.m_xSeries );
            if( aSeriesMapIt != aSeriesMapEnd && aSeriesMapIt->second < static_cast<sal_Int32>(aNewSeriesVector.size()) )
                aNewSeriesVector[aSeriesMapIt->second].mnAttachedAxis = aStyle.mnAttachedAxis;
        }
    }

    //overwrite new series style names with old series style name information
    for (auto const& style : rStyleVector)
    {
        DataRowPointStyle aStyle(style);
        if( aStyle.meType == DataRowPointStyle::DATA_SERIES )
        {
            auto aSeriesMapIt = aSeriesMap.find(aStyle.m_xSeries);
            if( aSeriesMapEnd != aSeriesMapIt )
            {
                sal_Int32 nNewPointIndex = aSeriesMapIt->second;

                for (auto & newSeries : aNewSeriesVector)
                    newSeries.setSeriesStyleNameToPoint( aStyle.msStyleName, nNewPointIndex );
            }
        }
    }

    //overwrite new series style names with point style name information
    for (auto const& style : rStyleVector)
    {
        DataRowPointStyle aStyle(style);
        if( aStyle.meType == DataRowPointStyle::DATA_POINT )
        {
            auto aSeriesMapIt = aSeriesMap.find(aStyle.m_xSeries);
            if( aSeriesMapEnd != aSeriesMapIt )
            {
                sal_Int32 nNewPointIndex = aSeriesMapIt->second;
                sal_Int32 nNewSeriesIndex = aStyle.m_nPointIndex;
                sal_Int32 nRepeatCount = aStyle.m_nPointRepeat;

                while( nRepeatCount && (nNewSeriesIndex>=0) && (o3tl::make_unsigned(nNewSeriesIndex)< aNewSeriesVector.size() ) )
                {
                    NewDonutSeries& rNewSeries( aNewSeriesVector[nNewSeriesIndex] );
                    rNewSeries.setPointStyleNameToPoint( aStyle.msStyleName, nNewPointIndex );

                    nRepeatCount--;
                    nNewSeriesIndex++;
                }
            }
        }
    }

    //put information from aNewSeriesVector to output parameter rStyleVector
    rStyleVector.clear();

    for (auto & newSeries : aNewSeriesVector)
    {
        ::std::vector< DataRowPointStyle > aVector( newSeries.creatStyleVector() );
        rStyleVector.insert(rStyleVector.end(),aVector.begin(),aVector.end());
    }
}

bool lcl_SpecialHandlingForDonutChartNeeded(
    std::u16string_view rServiceName,
    const SvXMLImport & rImport )
{
    bool bResult = false;
    if( rServiceName == u"com.sun.star.chart2.DonutChartType" )
    {
        bResult = SchXMLTools::isDocumentGeneratedWithOpenOfficeOlderThan2_3( rImport.GetModel() );
    }
    return bResult;
}

} // anonymous namespace

static void lcl_ApplyDataFromRectangularRangeToDiagram(
        const uno::Reference< chart2::XChartDocument >& xNewDoc
        , const OUString& rRectangularRange
        , css::chart::ChartDataRowSource eDataRowSource
        , bool bRowHasLabels, bool bColHasLabels
        , bool bSwitchOnLabelsAndCategoriesForOwnData
        , std::u16string_view sColTrans
        , std::u16string_view sRowTrans )
{
    if( !xNewDoc.is() )
        return;

    uno::Reference< chart2::XDiagram > xNewDia( xNewDoc->getFirstDiagram());
    uno::Reference< chart2::data::XDataProvider > xDataProvider( xNewDoc->getDataProvider() );
    if( !xNewDia.is() || !xDataProvider.is() )
        return;

    bool bFirstCellAsLabel =
        (eDataRowSource==chart::ChartDataRowSource_COLUMNS)? bRowHasLabels : bColHasLabels;
    bool bHasCateories =
        (eDataRowSource==chart::ChartDataRowSource_COLUMNS)? bColHasLabels : bRowHasLabels;

    if( bSwitchOnLabelsAndCategoriesForOwnData )
    {
        bFirstCellAsLabel = true;
        bHasCateories = true;
    }

    uno::Sequence< beans::PropertyValue > aArgs{
        beans::PropertyValue(
           u"CellRangeRepresentation"_ustr,
           -1, uno::Any( rRectangularRange ),
           beans::PropertyState_DIRECT_VALUE ),
        beans::PropertyValue(
           u"DataRowSource"_ustr,
           -1, uno::Any( eDataRowSource ),
           beans::PropertyState_DIRECT_VALUE ),
        beans::PropertyValue(
           u"FirstCellAsLabel"_ustr,
           -1, uno::Any( bFirstCellAsLabel ),
           beans::PropertyState_DIRECT_VALUE )
    };

    if( !sColTrans.empty() || !sRowTrans.empty() )
    {
        aArgs.realloc( aArgs.getLength() + 1 );
        aArgs.getArray()[ sal::static_int_cast<sal_uInt32>(aArgs.getLength()) - 1 ] = beans::PropertyValue(
            u"SequenceMapping"_ustr,
            -1, uno::Any( !sColTrans.empty()
                ? lcl_getNumberSequenceFromString( sColTrans, bHasCateories && !xNewDoc->hasInternalDataProvider() )
                : lcl_getNumberSequenceFromString( sRowTrans, bHasCateories && !xNewDoc->hasInternalDataProvider() ) ),
        beans::PropertyState_DIRECT_VALUE );
    }

    //work around wrong writer ranges ( see Issue 58464 )
    {
        OUString aChartOleObjectName;
        if( xNewDoc.is() )
        {
            utl::MediaDescriptor aMediaDescriptor( xNewDoc->getArgs() );

            utl::MediaDescriptor::const_iterator aIt(
                aMediaDescriptor.find( u"HierarchicalDocumentName"_ustr));
            if( aIt != aMediaDescriptor.end() )
            {
                aChartOleObjectName = (*aIt).second.get< OUString >();
            }
        }
        if( !aChartOleObjectName.isEmpty() )
        {
            aArgs.realloc( aArgs.getLength() + 1 );
            aArgs.getArray()[ sal::static_int_cast<sal_uInt32>(aArgs.getLength()) - 1 ] = beans::PropertyValue(
                u"ChartOleObjectName"_ustr,
                -1, uno::Any( aChartOleObjectName ),
                beans::PropertyState_DIRECT_VALUE );
        }
    }

    uno::Reference< chart2::data::XDataSource > xDataSource(
        xDataProvider->createDataSource( aArgs ));

    aArgs.realloc( aArgs.getLength() + 2 );
    auto pArgs = aArgs.getArray();
    pArgs[ sal::static_int_cast<sal_uInt32>(aArgs.getLength()) - 2 ] = beans::PropertyValue(
        u"HasCategories"_ustr,
        -1, uno::Any( bHasCateories ),
        beans::PropertyState_DIRECT_VALUE );
    pArgs[ sal::static_int_cast<sal_uInt32>(aArgs.getLength()) - 1 ] = beans::PropertyValue(
        u"UseCategoriesAsX"_ustr,
        -1, uno::Any( false ),//categories in ODF files are not to be used as x values (independent from what is offered in our ui)
        beans::PropertyState_DIRECT_VALUE );

    xNewDia->setDiagramData( xDataSource, aArgs );
}

void SchXMLChartContext::endFastElement(sal_Int32 )
{
    uno::Reference< chart::XChartDocument > xDoc = mrImportHelper.GetChartDocument();
    uno::Reference< beans::XPropertySet > xProp( xDoc, uno::UNO_QUERY );
    uno::Reference< chart2::XChartDocument > xNewDoc( xDoc, uno::UNO_QUERY );

    if( xProp.is())
    {
        if( !maMainTitle.empty())
        {
            uno::Reference< beans::XPropertySet > xTitleProp(xDoc->getTitle(), uno::UNO_QUERY);
            SchXMLTools::importFormattedText(GetImport(), maMainTitle, xTitleProp);
        }

        if( !maSubTitle.empty())
        {
            uno::Reference< beans::XPropertySet > xTitleProp(xDoc->getSubTitle(), uno::UNO_QUERY);
            SchXMLTools::importFormattedText(GetImport(), maSubTitle, xTitleProp);
        }
    }

    // cleanup: remove empty chart type groups
    lcl_removeEmptyChartTypeGroups( xNewDoc );

    // Handle of-pie parameters. Is this the right place to do this?
    if (maChartTypeServiceName == "com.sun.star.chart2.PieChartType") {
        Reference< chart2::XDiagram> xDia(xNewDoc->getFirstDiagram());
        uno::Reference< beans::XPropertySet > xDiaProp( xDia, uno::UNO_QUERY );
        if( xDiaProp.is()) {
            xDiaProp->setPropertyValue(u"SubPieType"_ustr, uno::Any(mPieSubType));
            xDiaProp->setPropertyValue(u"SplitPos"_ustr, uno::Any(mfPieSplitPos));
        }
    }

    // set stack mode before a potential chart type detection (in case we have a rectangular range)
    uno::Reference< chart::XDiagram > xDiagram( xDoc->getDiagram() );
    uno::Reference< beans::XPropertySet > xDiaProp( xDiagram, uno::UNO_QUERY );
    if( xDiaProp.is())
    {
        if( maSeriesDefaultsAndStyles.maStackedDefault.hasValue())
            xDiaProp->setPropertyValue(u"Stacked"_ustr,maSeriesDefaultsAndStyles.maStackedDefault);
        if( maSeriesDefaultsAndStyles.maPercentDefault.hasValue())
            xDiaProp->setPropertyValue(u"Percent"_ustr,maSeriesDefaultsAndStyles.maPercentDefault);
        if( maSeriesDefaultsAndStyles.maDeepDefault.hasValue())
            xDiaProp->setPropertyValue(u"Deep"_ustr,maSeriesDefaultsAndStyles.maDeepDefault);
        if( maSeriesDefaultsAndStyles.maStackedBarsConnectedDefault.hasValue())
            xDiaProp->setPropertyValue(u"StackedBarsConnected"_ustr,maSeriesDefaultsAndStyles.maStackedBarsConnectedDefault);
    }

    //the OOo 2.0 implementation and older has a bug with donuts
    bool bSpecialHandlingForDonutChart = lcl_SpecialHandlingForDonutChartNeeded(
        maChartTypeServiceName, GetImport());

    // apply data
    if(!xNewDoc.is())
        return;

    bool bHasOwnData = false;
    if( m_aXLinkHRefAttributeToIndicateDataProvider == "." ) //data comes from the chart itself
        bHasOwnData = true;
    else if( m_aXLinkHRefAttributeToIndicateDataProvider == ".." ) //data comes from the parent application
        bHasOwnData = false;
    else if( !m_aXLinkHRefAttributeToIndicateDataProvider.isEmpty() ) //not supported so far to get the data by sibling objects -> fall back to chart itself if data are available
        bHasOwnData = m_bHasTableElement;
    else
        bHasOwnData = !m_bHasRangeAtPlotArea;

    if( xNewDoc->hasInternalDataProvider())
    {
        if( !m_bHasTableElement && m_aXLinkHRefAttributeToIndicateDataProvider != "." )
        {
            //#i103147# ODF, workaround broken files with a missing table:cell-range-address at the plot-area
            bool bSwitchSuccessful = SchXMLTools::switchBackToDataProviderFromParent( xNewDoc, maLSequencesPerIndex );
            bHasOwnData = !bSwitchSuccessful;
        }
        else
            bHasOwnData = true;//e.g. in case of copy->paste from calc to impress
    }
    else if( bHasOwnData )
    {
        xNewDoc->createInternalDataProvider( false /* bCloneExistingData */ );
    }
    if( bHasOwnData )
        msChartAddress = "all";

    bool bSwitchRangesFromOuterToInternalIfNecessary = false;
    if(!bHasOwnData && mbIsStockChart)
    {
        // special handling for stock chart (merge series together)
        MergeSeriesForStockChart();
    }
    else if((bHasOwnData || !mbAllRangeAddressesAvailable) && !msChartAddress.isEmpty())
    {
        //own data or only rectangular range available

        if( xNewDoc->hasInternalDataProvider() )
            SchXMLTableHelper::applyTableToInternalDataProvider( maTable, xNewDoc );

        bool bOlderThan2_3 = SchXMLTools::isDocumentGeneratedWithOpenOfficeOlderThan2_3( xNewDoc );
        bool bOldFileWithOwnDataFromRows = (bOlderThan2_3 && bHasOwnData && (meDataRowSource==chart::ChartDataRowSource_ROWS)); // in this case there are range addresses that are simply wrong.

        if( mbAllRangeAddressesAvailable && !bSpecialHandlingForDonutChart && !mbIsStockChart &&
            !bOldFileWithOwnDataFromRows )
        {
            //bHasOwnData is true in this case!
            //e.g. for normal files with own data or also in case of copy paste scenario (e.g. calc to impress)
            bSwitchRangesFromOuterToInternalIfNecessary = true;
        }
        else
        {
            //apply data from rectangular range

            // create datasource from data provider with rectangular range parameters and change the diagram setDiagramData
            try
            {
                if( bOlderThan2_3 && xDiaProp.is() )//for older charts the hidden cells were removed by calc on the fly
                    xDiaProp->setPropertyValue(u"IncludeHiddenCells"_ustr,uno::Any(false));

                // note: mbRowHasLabels means the first row contains labels, that means we have "column-descriptions",
                // (analogously mbColHasLabels means we have "row-descriptions")
                lcl_ApplyDataFromRectangularRangeToDiagram( xNewDoc, msChartAddress, meDataRowSource, mbRowHasLabels, mbColHasLabels, bHasOwnData, msColTrans, msRowTrans );
            }
            catch(const uno::Exception&)
            {
                //try to fallback to internal data
                TOOLS_WARN_EXCEPTION("xmloff.chart", "Exception during import SchXMLChartContext::lcl_ApplyDataFromRectangularRangeToDiagram try to fallback to internal data" );
                if(!bHasOwnData)
                {
                    bHasOwnData = true;
                    msChartAddress = "all";
                    if( !xNewDoc->hasInternalDataProvider() )
                    {
                        xNewDoc->createInternalDataProvider( false /* bCloneExistingData */ );
                        SchXMLTableHelper::applyTableToInternalDataProvider( maTable, xNewDoc );
                        try
                        {
                            lcl_ApplyDataFromRectangularRangeToDiagram( xNewDoc, msChartAddress, meDataRowSource, mbRowHasLabels, mbColHasLabels, bHasOwnData, msColTrans, msRowTrans );
                        }
                        catch(const uno::Exception&)
                        {
                            TOOLS_WARN_EXCEPTION("xmloff.chart", "Exception during import SchXMLChartContext::lcl_ApplyDataFromRectangularRangeToDiagram fallback to internal data failed also" );
                        }
                    }
                }
            }
        }
    }
    else
    {
        SAL_WARN("xmloff.chart", "Must not get here" );
    }

    // now all series and data point properties are available and can be set
    {
        if( bSpecialHandlingForDonutChart )
        {
            uno::Reference< chart2::XDiagram > xNewDiagram( xNewDoc->getFirstDiagram() );
            lcl_swapPointAndSeriesStylesForDonutCharts( maSeriesDefaultsAndStyles.maSeriesStyleVector
                , SchXMLSeriesHelper::getDataSeriesIndexMapFromDiagram(xNewDiagram) );
        }

        SchXMLSeries2Context::initSeriesPropertySets( maSeriesDefaultsAndStyles, xDoc );

        //set defaults from diagram to the new series:
        //check whether we need to remove lines from symbol only charts
        bool bSwitchOffLinesForScatter = false;
        {
            bool bLinesOn = true;
            if( (maSeriesDefaultsAndStyles.maLinesOnProperty >>= bLinesOn) && !bLinesOn )
            {
                if( maChartTypeServiceName == "com.sun.star.chart2.ScatterChartType" )
                {
                    bSwitchOffLinesForScatter = true;
                    SchXMLSeries2Context::switchSeriesLinesOff( maSeriesDefaultsAndStyles.maSeriesStyleVector );
                }
            }
        }
        SchXMLSeries2Context::setDefaultsToSeries( maSeriesDefaultsAndStyles );

        // set autostyles for series and data points
        const SvXMLStylesContext* pStylesCtxt = mrImportHelper.GetAutoStylesContext();
        const SvXMLStyleContext* pStyle = nullptr;
        OUString sCurrStyleName;

        if( pStylesCtxt )
        {
            //iterate over data-series first
            //don't set series styles for donut charts
            if( !bSpecialHandlingForDonutChart )
            {
                SchXMLSeries2Context::setStylesToSeries(
                                        maSeriesDefaultsAndStyles, pStylesCtxt, pStyle,
                                        sCurrStyleName, mrImportHelper, GetImport(),
                                        mbIsStockChart, maLSequencesPerIndex );
                // ... then set attributes for statistics (after their existence was set in the series)
                SchXMLSeries2Context::setStylesToStatisticsObjects(
                                        maSeriesDefaultsAndStyles, pStylesCtxt,
                                        pStyle, sCurrStyleName );

                SchXMLSeries2Context::setStylesToRegressionCurves(
                                        maSeriesDefaultsAndStyles, pStylesCtxt,
                                        pStyle, sCurrStyleName );
            }
        }

        //#i98319# call switchRangesFromOuterToInternalIfNecessary before the data point styles are applied, otherwise in copy->paste scenario the data point styles do get lost
        if( bSwitchRangesFromOuterToInternalIfNecessary )
        {
            if( xNewDoc->hasInternalDataProvider() )
                SchXMLTableHelper::switchRangesFromOuterToInternalIfNecessary( maTable, maLSequencesPerIndex, xNewDoc, meDataRowSource );
        }

        if( pStylesCtxt )
        {
            // ... then iterate over data-point attributes, so the latter are not overwritten
            SchXMLSeries2Context::setStylesToDataPoints( maSeriesDefaultsAndStyles
                            , pStylesCtxt, pStyle, sCurrStyleName, mrImportHelper, GetImport(), mbIsStockChart, bSpecialHandlingForDonutChart, bSwitchOffLinesForScatter );
        }
    }

    if( xProp.is())
        xProp->setPropertyValue(u"RefreshAddInAllowed"_ustr, uno::Any( true) );
}

void SchXMLChartContext::MergeSeriesForStockChart()
{
    OSL_ASSERT( mbIsStockChart );
    try
    {
        uno::Reference< chart::XChartDocument > xOldDoc( mrImportHelper.GetChartDocument());
        uno::Reference< chart2::XChartDocument > xDoc( xOldDoc, uno::UNO_QUERY_THROW );
        uno::Reference< chart2::XDiagram > xDiagram( xDoc->getFirstDiagram());
        if( ! xDiagram.is())
            return;

        bool bHasJapaneseCandlestick = true;
        uno::Reference< chart2::XDataSeriesContainer > xDSContainer;
        uno::Reference< chart2::XCoordinateSystemContainer > xCooSysCnt( xDiagram, uno::UNO_QUERY_THROW );
        const uno::Sequence< uno::Reference< chart2::XCoordinateSystem > > aCooSysSeq( xCooSysCnt->getCoordinateSystems());
        for( const auto& rCooSys : aCooSysSeq )
        {
            uno::Reference< chart2::XChartTypeContainer > xCTCnt( rCooSys, uno::UNO_QUERY_THROW );
            const uno::Sequence< uno::Reference< chart2::XChartType > > aChartTypes( xCTCnt->getChartTypes());
            auto pChartType = std::find_if(aChartTypes.begin(), aChartTypes.end(),
                [](const auto& rChartType) { return rChartType->getChartType() == "com.sun.star.chart2.CandleStickChartType"; });
            if (pChartType != aChartTypes.end())
            {
                xDSContainer.set( *pChartType, uno::UNO_QUERY_THROW );
                uno::Reference< beans::XPropertySet > xCTProp( *pChartType, uno::UNO_QUERY_THROW );
                xCTProp->getPropertyValue(u"Japanese"_ustr) >>= bHasJapaneseCandlestick;
            }
        }

        if( xDSContainer.is())
        {
            // with japanese candlesticks: open, low, high, close
            // otherwise: low, high, close
            uno::Sequence< uno::Reference< chart2::XDataSeries > > aSeriesSeq( xDSContainer->getDataSeries());
            const sal_Int32 nSeriesCount( aSeriesSeq.getLength());
            const sal_Int32 nSeriesPerCandleStick = bHasJapaneseCandlestick ? 4: 3;
            sal_Int32 nCandleStickCount = nSeriesCount / nSeriesPerCandleStick;
            OSL_ASSERT( nSeriesPerCandleStick * nCandleStickCount == nSeriesCount );
            uno::Sequence< uno::Reference< chart2::XDataSeries > > aNewSeries( nCandleStickCount );
            auto aNewSeriesRange = asNonConstRange(aNewSeries);
            for( sal_Int32 i=0; i<nCandleStickCount; ++i )
            {
                sal_Int32 nSeriesIndex = i*nSeriesPerCandleStick;
                if( bHasJapaneseCandlestick )
                {
                    // open values
                    lcl_setRoleAtFirstSequence( aSeriesSeq[ nSeriesIndex ], u"values-first"_ustr);
                    aNewSeriesRange[i] = aSeriesSeq[ nSeriesIndex ];
                    // low values
                    lcl_MoveDataToCandleStickSeries(
                        uno::Reference< chart2::data::XDataSource >( aSeriesSeq[ ++nSeriesIndex ], uno::UNO_QUERY_THROW ),
                        aNewSeries[i], u"values-min"_ustr);
                }
                else
                {
                    // low values
                    lcl_setRoleAtFirstSequence( aSeriesSeq[ nSeriesIndex ], u"values-min"_ustr);
                    aNewSeriesRange[i] = aSeriesSeq[ nSeriesIndex ];
                }
                // high values
                lcl_MoveDataToCandleStickSeries(
                    uno::Reference< chart2::data::XDataSource >( aSeriesSeq[ ++nSeriesIndex ], uno::UNO_QUERY_THROW ),
                    aNewSeries[i], u"values-max"_ustr);
                // close values
                lcl_MoveDataToCandleStickSeries(
                    uno::Reference< chart2::data::XDataSource >( aSeriesSeq[ ++nSeriesIndex ], uno::UNO_QUERY_THROW ),
                    aNewSeries[i], u"values-last"_ustr);
            }
            xDSContainer->setDataSeries( aNewSeries );
        }
    }
    catch(const uno::Exception&)
    {
        TOOLS_WARN_EXCEPTION("xmloff.chart", "Exception while merging series for stock chart" );
    }
}

css::uno::Reference< css::xml::sax::XFastContextHandler > SchXMLChartContext::createFastChildContext(
    sal_Int32 nElement,
    const css::uno::Reference< css::xml::sax::XFastAttributeList >& xAttrList )
{
    SvXMLImportContext* pContext = nullptr;
    uno::Reference< chart::XChartDocument > xDoc = mrImportHelper.GetChartDocument();
    uno::Reference< beans::XPropertySet > xProp( xDoc, uno::UNO_QUERY );

    switch(nElement)
    {
        case XML_ELEMENT(CHART, XML_PLOT_AREA):
            pContext = new SchXMLPlotAreaContext( mrImportHelper, GetImport(),
                                                  m_aXLinkHRefAttributeToIndicateDataProvider,
                                                  msCategoriesAddress,
                                                  msChartAddress, m_bHasRangeAtPlotArea, mbAllRangeAddressesAvailable,
                                                  mbColHasLabels, mbRowHasLabels,
                                                  meDataRowSource,
                                                  maSeriesDefaultsAndStyles,
                                                  maChartTypeServiceName,
                                                  maLSequencesPerIndex, maChartSize );
            break;
        case XML_ELEMENT(CHART, XML_TITLE):
            if( xDoc.is())
            {
                if( xProp.is())
                {
                    xProp->setPropertyValue(u"HasMainTitle"_ustr, uno::Any(true) );
                }
                pContext = new SchXMLTitleContext( mrImportHelper, GetImport(),
                                                   maMainTitle, xDoc->getTitle() );
            }
            break;
        case XML_ELEMENT(CHART, XML_SUBTITLE):
            if( xDoc.is())
            {
                if( xProp.is())
                {
                    xProp->setPropertyValue(u"HasSubTitle"_ustr, uno::Any(true) );
                }
                pContext = new SchXMLTitleContext( mrImportHelper, GetImport(),
                                                   maSubTitle, xDoc->getSubTitle() );
            }
            break;
        case XML_ELEMENT(CHART, XML_LEGEND):
            pContext = new SchXMLLegendContext( mrImportHelper, GetImport() );
            break;
        case XML_ELEMENT(LO_EXT, XML_DATA_TABLE):
            pContext = new SchXMLDataTableContext(mrImportHelper, GetImport());
            break;
        case XML_ELEMENT(TABLE, XML_TABLE):
            {
                SchXMLTableContext * pTableContext =
                    new SchXMLTableContext( GetImport(), maTable );
                m_bHasTableElement = true;
                // #i85913# take into account column- and row- mapping for
                // charts with own data only for those which were not copied
                // from a place where they got data from the container.  Note,
                // that this requires the plot-area been read before the table
                // (which is required in the ODF spec)
                // Note: For stock charts and donut charts with special handling
                // the mapping must not be applied!
                if( msChartAddress.isEmpty() && !mbIsStockChart &&
                    !lcl_SpecialHandlingForDonutChartNeeded(
                        maChartTypeServiceName, GetImport()))
                {
                    if( !msColTrans.isEmpty() )
                    {
                        OSL_ASSERT( msRowTrans.isEmpty() );
                        pTableContext->setColumnPermutation( lcl_getNumberSequenceFromString( msColTrans, true ));
                        msColTrans.clear();
                    }
                    else if( !msRowTrans.isEmpty() )
                    {
                        pTableContext->setRowPermutation( lcl_getNumberSequenceFromString( msRowTrans, true ));
                        msRowTrans.clear();
                    }
                }
                pContext = pTableContext;
            }
            break;

        default:
            // try importing as an additional shape
            if( ! mxDrawPage.is())
            {
                uno::Reference< drawing::XDrawPageSupplier  > xSupp( xDoc, uno::UNO_QUERY );
                if( xSupp.is())
                    mxDrawPage = xSupp->getDrawPage();

                SAL_WARN_IF( !mxDrawPage.is(), "xmloff.chart", "Invalid Chart Page" );
            }
            if( mxDrawPage.is())
                pContext = XMLShapeImportHelper::CreateGroupChildContext(
                    GetImport(), nElement, xAttrList, mxDrawPage );
            break;
    }

    return pContext;
}

/*
    With a locked controller the following is done here:
        1.  Hide title, subtitle, and legend.
        2.  Set the size of the draw page.
        3.  Set a (logically) empty data set.
        4.  Set the chart type.
*/
void SchXMLChartContext::InitChart(
    const OUString & rChartTypeServiceName // currently the old service name
    )
{
    uno::Reference< chart::XChartDocument > xDoc = mrImportHelper.GetChartDocument();
    SAL_WARN_IF( !xDoc.is(), "xmloff.chart", "No valid document!" );

    // Remove Title and Diagram ("De-InitNew")
    uno::Reference< chart2::XChartDocument > xNewDoc( mrImportHelper.GetChartDocument(), uno::UNO_QUERY );
    if( xNewDoc.is())
    {
        xNewDoc->setFirstDiagram( nullptr );
        uno::Reference< chart2::XTitled > xTitled( xNewDoc, uno::UNO_QUERY );
        if( xTitled.is())
            xTitled->setTitleObject( nullptr );
    }

    //  Set the chart type via setting the diagram.
    if( !rChartTypeServiceName.isEmpty() && xDoc.is())
    {
        uno::Reference< lang::XMultiServiceFactory > xFact( xDoc, uno::UNO_QUERY );
        if( xFact.is())
        {
            uno::Reference< chart::XDiagram > xDia( xFact->createInstance( rChartTypeServiceName ), uno::UNO_QUERY );
            if( xDia.is())
                xDoc->setDiagram( xDia );
        }
    }
}

SchXMLTitleContext::SchXMLTitleContext( SchXMLImportHelper& rImpHelper, SvXMLImport& rImport,
                                        std::vector<std::pair<OUString, OUString>>& rTitle,
                                        uno::Reference< drawing::XShape > xTitleShape ) :
        SvXMLImportContext( rImport ),
        mrImportHelper( rImpHelper ),
        mrTitle( rTitle ),
        mxTitleShape(std::move( xTitleShape ))
{
}

SchXMLTitleContext::~SchXMLTitleContext()
{}

void SchXMLTitleContext::startFastElement( sal_Int32 /*nElement*/,
    const css::uno::Reference< css::xml::sax::XFastAttributeList >& xAttrList )
{
    css::awt::Point aPosition;
    bool bHasXPosition=false;
    bool bHasYPosition=false;

    for( auto& aIter : sax_fastparser::castToFastAttributeList(xAttrList) )
    {
        switch (aIter.getToken())
        {
            case XML_ELEMENT(SVG, XML_X):
            case XML_ELEMENT(SVG_COMPAT, XML_X):
            {
                GetImport().GetMM100UnitConverter().convertMeasureToCore(
                        aPosition.X, aIter.toView() );
                bHasXPosition = true;
                break;
            }
            case XML_ELEMENT(SVG, XML_Y):
            case XML_ELEMENT(SVG_COMPAT, XML_Y):
            {
                GetImport().GetMM100UnitConverter().convertMeasureToCore(
                        aPosition.Y, aIter.toView() );
                bHasYPosition = true;
                break;
            }
            case XML_ELEMENT(CHART, XML_STYLE_NAME):
                msAutoStyleName = aIter.toString();
                break;
            default:
                XMLOFF_WARN_UNKNOWN("xmloff", aIter);
        }
    }

    if( mxTitleShape.is())
    {
        if( bHasXPosition && bHasYPosition )
            mxTitleShape->setPosition( aPosition );

        uno::Reference<beans::XPropertySet> xProp(mxTitleShape, uno::UNO_QUERY);
        mrImportHelper.FillAutoStyle(msAutoStyleName, xProp);
    }
}

css::uno::Reference< css::xml::sax::XFastContextHandler > SchXMLTitleContext::createFastChildContext(
    sal_Int32 nElement,
    const css::uno::Reference< css::xml::sax::XFastAttributeList >& /*xAttrList*/ )
{
    SvXMLImportContext* pContext = nullptr;

    if( nElement == XML_ELEMENT(TEXT, XML_P) ||
        nElement == XML_ELEMENT(LO_EXT, XML_P) )
    {
        pContext = new SchXMLTitleParaContext(GetImport(), mrTitle);
    }
    else
        XMLOFF_WARN_UNKNOWN_ELEMENT("xmloff", nElement);

    return pContext;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
