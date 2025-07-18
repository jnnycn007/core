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

#include <comphelper/servicehelper.hxx>
#include <utility>
#include <vcl/svapp.hxx>
#include <osl/mutex.hxx>
#include <svl/itemprop.hxx>
#include <svx/dataaccessdescriptor.hxx>
#include <unotools/tempfile.hxx>
#include <sfx2/app.hxx>
#include <sfx2/docfilt.hxx>
#include <tools/urlobj.hxx>
#include <comphelper/diagnose_ex.hxx>
#include <comphelper/processfactory.hxx>
#include <comphelper/propertyvalue.hxx>
#include <comphelper/string.hxx>
#include <cppuhelper/supportsservice.hxx>
#include <vcl/timer.hxx>
#include <com/sun/star/sdb/CommandType.hpp>
#include <com/sun/star/text/MailMergeType.hpp>
#include <com/sun/star/text/MailMergeEvent.hpp>
#include <com/sun/star/text/XMailMergeListener.hpp>
#include <com/sun/star/beans/PropertyAttribute.hpp>
#include <com/sun/star/sdbc/XResultSet.hpp>
#include <com/sun/star/sdbc/XConnection.hpp>
#include <com/sun/star/sdbc/XRowSet.hpp>
#include <com/sun/star/frame/Desktop.hpp>
#include <com/sun/star/util/XCloseable.hpp>
#include <com/sun/star/util/CloseVetoException.hpp>
#include <com/sun/star/sdbcx/XRowLocate.hpp>
#include <com/sun/star/frame/XStorable.hpp>
#include <com/sun/star/mail/XSmtpService.hpp>
#include <sfx2/viewfrm.hxx>
#include <sfx2/event.hxx>
#include <cppuhelper/implbase.hxx>
#include <printdata.hxx>
#include <swevent.hxx>
#include <unomailmerge.hxx>
#include <unoprnms.hxx>
#include <unomap.hxx>
#include <swunohelper.hxx>
#include <docsh.hxx>
#include <IDocumentDeviceAccess.hxx>
#include <view.hxx>
#include <dbmgr.hxx>
#include <unotxdoc.hxx>
#include <wrtsh.hxx>
#include <mmconfigitem.hxx>
#include <mailmergehelper.hxx>

#include <iodetect.hxx>

#include <memory>

using namespace ::com::sun::star;
using namespace ::com::sun::star::frame;
using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::lang;
using namespace ::com::sun::star::beans;
using namespace ::com::sun::star::text;
using namespace SWUnoHelper;

typedef ::utl::SharedUNOComponent< XInterface > SharedComponent;

static osl::Mutex &    GetMailMergeMutex()
{
    static osl::Mutex   aMutex;
    return aMutex;
}

namespace {

enum CloseResult
{
    eSuccess,       // successfully closed
    eVetoed,        // vetoed, ownership transferred to the vetoing instance
    eFailed         // failed for some unknown reason
};

}

static CloseResult CloseModelAndDocSh(
       Reference< frame::XModel > const &rxModel,
       SfxObjectShellRef &rxDocSh )
{
    CloseResult eResult = eSuccess;

    rxDocSh = nullptr;

    //! models/documents should never be disposed (they may still be
    //! used for printing which is called asynchronously for example)
    //! instead call close
    Reference< util::XCloseable > xClose( rxModel, UNO_QUERY );
    if (xClose.is())
    {
        try
        {
            //! 'sal_True' -> transfer ownership to vetoing object if vetoed!
            //! I.e. now that object is responsible for closing the model and doc shell.
            xClose->close( true );
        }
        catch (const util::CloseVetoException&)
        {
            //! here we have the problem that the temporary file that is
            //! currently being printed will never be deleted. :-(
            eResult = eVetoed;
        }
        catch (const uno::RuntimeException&)
        {
            eResult = eFailed;
        }
    }
    return eResult;
}

/// @throws RuntimeException
static bool LoadFromURL_impl(
        Reference< frame::XModel > &rxModel,
        SfxObjectShellRef &rxDocSh,
        const OUString &rURL,
        bool bClose )
{
    // try to open the document readonly and hidden
    Reference< frame::XModel > xTmpModel;
    Sequence < PropertyValue > aArgs{ comphelper::makePropertyValue(u"Hidden"_ustr, true) };
    try
    {
        Reference < XDesktop2 > xDesktop = Desktop::create( ::comphelper::getProcessComponentContext() );
        xTmpModel.set( xDesktop->loadComponentFromURL( rURL, u"_blank"_ustr, 0, aArgs ), UNO_QUERY );
    }
    catch (const Exception&)
    {
        return false;
    }

    // try to get the DocShell
    SwDocShell *pTmpDocShell = nullptr;
    if (auto pTextDoc = comphelper::getFromUnoTunnel<SwXTextDocument>(xTmpModel); pTextDoc)
        pTmpDocShell = pTextDoc->GetDocShell();

    bool bRes = false;
    if (xTmpModel.is() && pTmpDocShell)    // everything available?
    {
        if (bClose)
            CloseModelAndDocSh( rxModel, rxDocSh );
        // set new stuff
        rxModel = std::move(xTmpModel);
        rxDocSh = pTmpDocShell;
        bRes = true;
    }
    else
    {
        // SfxObjectShellRef is ok here, since the document will be explicitly closed
        SfxObjectShellRef xTmpDocSh = pTmpDocShell;
        CloseModelAndDocSh( xTmpModel, xTmpDocSh );
    }

    return bRes;
}

namespace
{
    class DelayedFileDeletion : public ::cppu::WeakImplHelper<util::XCloseListener>
    {
    protected:
        ::osl::Mutex                    m_aMutex;
        Reference< util::XCloseable >   m_xDocument;
        Timer                           m_aDeleteTimer;
        OUString                        m_sTemporaryFile;
        sal_Int32                       m_nPendingDeleteAttempts;

        DelayedFileDeletion(DelayedFileDeletion const&) = delete;
        DelayedFileDeletion& operator=(DelayedFileDeletion const&) = delete;

    public:
        DelayedFileDeletion( const Reference< XModel >& _rxModel,
                             OUString _aTemporaryFile );

    protected:
        virtual ~DelayedFileDeletion( ) override;

        // XCloseListener
        virtual void SAL_CALL queryClosing( const EventObject& _rSource, sal_Bool _bGetsOwnership ) override;
        virtual void SAL_CALL notifyClosing( const EventObject& _rSource ) override;

        // XEventListener
        virtual void SAL_CALL disposing( const EventObject& Source ) override;

    private:
        void implTakeOwnership( );
        DECL_LINK( OnTryDeleteFile, Timer*, void );
    };

    DelayedFileDeletion::DelayedFileDeletion( const Reference< XModel >& _rxModel, OUString  _aTemporaryFile )
        :
        m_xDocument( _rxModel, UNO_QUERY )
        ,m_aDeleteTimer("sw DelayedFileDeletion m_aDeleteTimer")
        ,m_sTemporaryFile(std::move( _aTemporaryFile ))
        ,m_nPendingDeleteAttempts( 0 )
    {
        osl_atomic_increment( &m_refCount );
        try
        {
            if ( m_xDocument.is() )
            {
                m_xDocument->addCloseListener( this );
                // successfully added -> keep ourself alive
                acquire();
            }
            else {
                OSL_FAIL("DelayedFileDeletion::DelayedFileDeletion: model is no component!" );
            }
        }
        catch (const Exception&)
        {
            OSL_FAIL("DelayedFileDeletion::DelayedFileDeletion: could not register as event listener at the model!" );
        }
        osl_atomic_decrement( &m_refCount );
    }

    IMPL_LINK_NOARG(DelayedFileDeletion, OnTryDeleteFile, Timer *, void)
    {
        ::osl::ClearableMutexGuard aGuard( m_aMutex );

        bool bSuccess = false;
        try
        {
            bool bDeliverOwnership = ( 0 == m_nPendingDeleteAttempts );
                // if this is our last attempt, then anybody which vetoes this has to take the consequences
                // (means take the ownership)
            m_xDocument->close( bDeliverOwnership );
            bSuccess = true;
        }
        catch (const util::CloseVetoException&)
        {
            // somebody vetoed -> next try
            if ( m_nPendingDeleteAttempts )
            {
                // next attempt
                --m_nPendingDeleteAttempts;
                m_aDeleteTimer.Start();
            }
            else
                bSuccess = true;    // can't do anything here ...
        }
        catch (const Exception&)
        {
            TOOLS_WARN_EXCEPTION( "sw", "DelayedFileDeletion::OnTryDeleteFile: caught a strange exception!" );
            bSuccess = true;
                // can't do anything here ...
        }

        if ( bSuccess )
        {
            SWUnoHelper::UCB_DeleteFile( m_sTemporaryFile );
            aGuard.clear();
            release();  // this should be our last reference, we should be dead after this
        }
    }

    void DelayedFileDeletion::implTakeOwnership( )
    {
        // revoke ourself as listener
        try
        {
            m_xDocument->removeCloseListener( this );
        }
        catch (const Exception&)
        {
            OSL_FAIL("DelayedFileDeletion::implTakeOwnership: could not revoke the listener!" );
        }

        m_aDeleteTimer.SetTimeout( 3000 );  // 3 seconds
        m_aDeleteTimer.SetInvokeHandler( LINK( this, DelayedFileDeletion, OnTryDeleteFile ) );
        m_nPendingDeleteAttempts = 3;   // try 3 times at most
        m_aDeleteTimer.Start( );
    }

    void SAL_CALL DelayedFileDeletion::queryClosing( const EventObject& , sal_Bool _bGetsOwnership )
    {
        ::osl::MutexGuard aGuard( m_aMutex );
        if ( _bGetsOwnership )
            implTakeOwnership( );

        // always veto: We want to take the ownership ourself, as this is the only chance to delete
        // the temporary file which the model is based on
        throw util::CloseVetoException( );
    }

    void SAL_CALL DelayedFileDeletion::notifyClosing( const EventObject&  )
    {
        OSL_FAIL("DelayedFileDeletion::notifyClosing: how this?" );
        // this should not happen:
        // Either, a foreign instance closes the document, then we should veto this, and take the ownership
        // Or, we ourself close the document, then we should not be a listener anymore
    }

    void SAL_CALL DelayedFileDeletion::disposing( const EventObject&  )
    {
        OSL_FAIL("DelayedFileDeletion::disposing: how this?" );
        // this should not happen:
        // Either, a foreign instance closes the document, then we should veto this, and take the ownership
        // Or, we ourself close the document, then we should not be a listener anymore
    }

    DelayedFileDeletion::~DelayedFileDeletion( )
    {
    }
}

static bool DeleteTmpFile_Impl(
        Reference< frame::XModel > &rxModel,
        SfxObjectShellRef &rxDocSh,
        const OUString &rTmpFileURL )
{
    bool bRes = false;
    if (!rTmpFileURL.isEmpty())
    {
        bool bDelete = true;
        if ( eVetoed == CloseModelAndDocSh( rxModel, rxDocSh ) )
        {
            // somebody vetoed the closing, and took the ownership of the document
            // -> ensure that the temporary file is deleted later on
            new DelayedFileDeletion( rxModel, rTmpFileURL );
                // note: as soon as #106931# is fixed, the whole DelayedFileDeletion is to be superseded by
                // a better solution
            bDelete = false;
        }

        rxModel = nullptr;
        rxDocSh = nullptr; // destroy doc shell

        if ( bDelete )
        {
            if ( !SWUnoHelper::UCB_DeleteFile( rTmpFileURL ) )
            {
                new DelayedFileDeletion( rxModel, rTmpFileURL );
                    // same not as above: as soon as #106931#, ...
            }
        }
        else
            bRes = true;    // file will be deleted delayed
    }
    return bRes;
}

SwXMailMerge::SwXMailMerge() :
    m_aEvtListeners   ( GetMailMergeMutex() ),
    m_aMergeListeners ( GetMailMergeMutex() ),
    m_aPropListeners  ( GetMailMergeMutex() ),
    m_pPropSet( aSwMapProvider.GetPropertySet( PROPERTY_MAP_MAILMERGE ) ),
    m_nDataCommandType(sdb::CommandType::TABLE),
    m_nOutputType(MailMergeType::PRINTER),
    m_bEscapeProcessing(true),     //!! allow to process properties like "Filter", "Order", ...
    m_bFileNameFromColumn(false),
    m_bSendAsHTML(false),
    m_bSendAsAttachment(false),
    m_bSaveAsSingleFile(false),
    m_bDisposing(false),
    m_pMgr(nullptr)
{
    // create empty document
    // like in: SwModule::InsertEnv (appenv.cxx)
    m_xDocSh = new SwDocShell( SfxObjectCreateMode::STANDARD );
    m_xDocSh->DoInitNew();
    SfxViewFrame *pFrame = SfxViewFrame::LoadHiddenDocument( *m_xDocSh, SFX_INTERFACE_NONE );
    SwView *pView = static_cast<SwView*>( pFrame->GetViewShell() );
    pView->AttrChangedNotify(nullptr); //So that SelectShell is called.
    m_xModel = m_xDocSh->GetModel();
}

SwXMailMerge::~SwXMailMerge()
{
    if (!m_aTmpFileName.isEmpty())
        DeleteTmpFile_Impl( m_xModel, m_xDocSh, m_aTmpFileName );
    else    // there was no temporary file in use
    {
        //! we still need to close the model and doc shell manually
        //! because there is no automatism that will do that later.
        //! #120086#
        if ( eVetoed == CloseModelAndDocSh( m_xModel, m_xDocSh ) )
            OSL_FAIL("ownership transferred to vetoing object!" );

        m_xModel = nullptr;
        m_xDocSh = nullptr; // destroy doc shell
    }
}

// Guarantee object consistence in case of an exception
class MailMergeExecuteFinalizer
{
public:
    explicit MailMergeExecuteFinalizer(SwXMailMerge *mailmerge)
        : m_pMailMerge(mailmerge)
    {
        assert(m_pMailMerge); //mailmerge object missing
    }
    ~MailMergeExecuteFinalizer()
    {
        osl::MutexGuard aMgrGuard( GetMailMergeMutex() );
        m_pMailMerge->m_pMgr = nullptr;
    }

private:
    MailMergeExecuteFinalizer(MailMergeExecuteFinalizer const&) = delete;
    MailMergeExecuteFinalizer& operator=(MailMergeExecuteFinalizer const&) = delete;

    SwXMailMerge *m_pMailMerge;
};

uno::Any SAL_CALL SwXMailMerge::execute(
        const uno::Sequence< beans::NamedValue >& rArguments )
{
    SolarMutexGuard aGuard;
    MailMergeExecuteFinalizer aFinalizer(this);

    // get property values to be used
    // (use values from the service as default and override them with
    // the values that are provided as arguments)

    uno::Sequence< uno::Any >           aCurSelection   = m_aSelection;
    uno::Reference< sdbc::XResultSet >  xCurResultSet   = m_xResultSet;
    uno::Reference< sdbc::XConnection > xCurConnection  = m_xConnection;
    uno::Reference< frame::XModel >     xCurModel       = m_xModel;
    OUString   aCurDataSourceName       = m_aDataSourceName;
    OUString   aCurDataCommand          = m_aDataCommand;
    OUString   aCurFilter               = m_aFilter;
    OUString   aCurDocumentURL          = m_aDocumentURL;
    OUString   aCurOutputURL            = m_aOutputURL;
    OUString   aCurFileNamePrefix       = m_aFileNamePrefix;
    sal_Int32  nCurDataCommandType      = m_nDataCommandType;
    sal_Int16  nCurOutputType           = m_nOutputType;
    bool   bCurEscapeProcessing     = m_bEscapeProcessing;
    bool   bCurFileNameFromColumn   = m_bFileNameFromColumn;

    SfxObjectShellRef xCurDocSh = m_xDocSh;   // the document

    for (const beans::NamedValue& rArgument : rArguments)
    {
        const OUString &rName   = rArgument.Name;
        const Any &rValue       = rArgument.Value;

        bool bOK = true;
        if (rName == UNO_NAME_SELECTION)
            bOK = rValue >>= aCurSelection;
        else if (rName == UNO_NAME_RESULT_SET)
            bOK = rValue >>= xCurResultSet;
        else if (rName == UNO_NAME_CONNECTION)
            bOK = rValue >>= xCurConnection;
        else if (rName == UNO_NAME_MODEL)
            throw PropertyVetoException("Property is read-only: " + rName, getXWeak() );
        else if (rName == UNO_NAME_DATA_SOURCE_NAME)
            bOK = rValue >>= aCurDataSourceName;
        else if (rName == UNO_NAME_DAD_COMMAND)
            bOK = rValue >>= aCurDataCommand;
        else if (rName == UNO_NAME_FILTER)
            bOK = rValue >>= aCurFilter;
        else if (rName == UNO_NAME_DOCUMENT_URL)
        {
            bOK = rValue >>= aCurDocumentURL;
            if (!aCurDocumentURL.isEmpty()
                && !LoadFromURL_impl( xCurModel, xCurDocSh, aCurDocumentURL, false ))
                throw RuntimeException("Failed to create document from URL: " + aCurDocumentURL, getXWeak() );
        }
        else if (rName == UNO_NAME_OUTPUT_URL)
        {
            bOK = rValue >>= aCurOutputURL;
            if (!aCurOutputURL.isEmpty())
            {
                if (!UCB_IsDirectory(aCurOutputURL))
                    throw IllegalArgumentException("URL does not point to a directory: " + aCurOutputURL, getXWeak(), 0 );
                if (UCB_IsReadOnlyFileName(aCurOutputURL))
                    throw IllegalArgumentException("URL is read-only: " + aCurOutputURL, getXWeak(), 0 );
            }
        }
        else if (rName == UNO_NAME_FILE_NAME_PREFIX)
            bOK = rValue >>= aCurFileNamePrefix;
        else if (rName == UNO_NAME_DAD_COMMAND_TYPE)
            bOK = rValue >>= nCurDataCommandType;
        else if (rName == UNO_NAME_OUTPUT_TYPE)
            bOK = rValue >>= nCurOutputType;
        else if (rName == UNO_NAME_ESCAPE_PROCESSING)
            bOK = rValue >>= bCurEscapeProcessing;
        else if (rName == UNO_NAME_FILE_NAME_FROM_COLUMN)
            bOK = rValue >>= bCurFileNameFromColumn;
        else if (rName == UNO_NAME_SUBJECT)
            bOK = rValue >>= m_sSubject;
        else if (rName == UNO_NAME_ADDRESS_FROM_COLUMN)
            bOK = rValue >>= m_sAddressFromColumn;
        else if (rName == UNO_NAME_SEND_AS_HTML)
            bOK = rValue >>= m_bSendAsHTML;
        else if (rName == UNO_NAME_MAIL_BODY)
            bOK = rValue >>= m_sMailBody;
        else if (rName == UNO_NAME_ATTACHMENT_NAME)
            bOK = rValue >>= m_sAttachmentName;
        else if (rName == UNO_NAME_ATTACHMENT_FILTER)
            bOK = rValue >>= m_sAttachmentFilter;
        else if (rName == UNO_NAME_COPIES_TO)
            bOK = rValue >>= m_aCopiesTo;
        else if (rName == UNO_NAME_BLIND_COPIES_TO)
            bOK = rValue >>= m_aBlindCopiesTo;
        else if (rName == UNO_NAME_SEND_AS_ATTACHMENT)
            bOK = rValue >>= m_bSendAsAttachment;
        else if (rName == UNO_NAME_PRINT_OPTIONS)
            bOK = rValue >>= m_aPrintSettings;
        else if (rName == UNO_NAME_SAVE_AS_SINGLE_FILE)
            bOK = rValue >>= m_bSaveAsSingleFile;
        else if (rName == UNO_NAME_SAVE_FILTER)
            bOK = rValue >>= m_sSaveFilter;
        else if (rName == UNO_NAME_SAVE_FILTER_OPTIONS)
            bOK = rValue >>= m_sSaveFilterOptions;
        else if (rName == UNO_NAME_SAVE_FILTER_DATA)
            bOK = rValue >>= m_aSaveFilterData;
        else if (rName == UNO_NAME_IN_SERVER_PASSWORD)
            bOK = rValue >>= m_sInServerPassword;
        else if (rName == UNO_NAME_OUT_SERVER_PASSWORD)
            bOK = rValue >>= m_sOutServerPassword;
        else
            throw UnknownPropertyException( "Property is unknown: " + rName, getXWeak() );

        if (!bOK)
            throw IllegalArgumentException("Property type mismatch or property not set: " + rName, getXWeak(), 0 );
    }

    // need to translate the selection: the API here requires a sequence of bookmarks, but the Merge
    // method we will call below requires a sequence of indices.
    if ( aCurSelection.hasElements() )
    {
        Sequence< Any > aTranslated( aCurSelection.getLength() );

        bool bValid = false;
        Reference< sdbcx::XRowLocate > xRowLocate( xCurResultSet, UNO_QUERY );
        if ( xRowLocate.is() )
        {
            Any* pTranslated = aTranslated.getArray();

            try
            {
                bool bEverythingsFine = true;
                for (const Any& rBookmark : aCurSelection)
                {
                    bEverythingsFine = xRowLocate->moveToBookmark( rBookmark );
                    if ( !bEverythingsFine )
                        break;
                    *pTranslated <<= xCurResultSet->getRow();
                    ++pTranslated;
                }
                if ( bEverythingsFine )
                    bValid = true;
            }
            catch (const Exception&)
            {
                bValid = false;
            }
        }

        if ( !bValid )
        {
            throw IllegalArgumentException(
                u"The current 'Selection' does not describe a valid array of bookmarks, relative to the current 'ResultSet'."_ustr,
                getXWeak(),
                0
            );
        }

        aCurSelection = std::move(aTranslated);
    }

    SfxViewFrame*   pFrame = SfxViewFrame::GetFirst( xCurDocSh.get(), false);
    SwView *pView = pFrame ? dynamic_cast<SwView*>( pFrame->GetViewShell()  ) : nullptr;
    if (!pView)
        throw RuntimeException();

    // avoid assertion in 'Update' from Sfx by supplying a shell
    // and thus avoiding the SelectShell call in Writers GetState function
    // while still in Update of Sfx.
    // (GetSelection in Update is not allowed)
    if (!aCurDocumentURL.isEmpty())
        pView->AttrChangedNotify(nullptr);//So that SelectShell is called.

    SharedComponent aRowSetDisposeHelper;
    if (!xCurResultSet.is())
    {
        if (aCurDataSourceName.isEmpty() || aCurDataCommand.isEmpty() )
        {
            OSL_FAIL("PropertyValues missing or unset");
            throw IllegalArgumentException(u"Either the ResultSet or DataSourceName and DataCommand must be set."_ustr, getXWeak(), 0 );
        }

        // build ResultSet from DataSourceName, DataCommand and DataCommandType

        Reference< XMultiServiceFactory > xMgr( ::comphelper::getProcessServiceFactory() );
        if (xMgr.is())
        {
            Reference< XInterface > xInstance = xMgr->createInstance( u"com.sun.star.sdb.RowSet"_ustr );
            aRowSetDisposeHelper.reset( xInstance, SharedComponent::TakeOwnership );
            Reference< XPropertySet > xRowSetPropSet( xInstance, UNO_QUERY );
            OSL_ENSURE( xRowSetPropSet.is(), "failed to get XPropertySet interface from RowSet" );
            if (xRowSetPropSet.is())
            {
                if (xCurConnection.is())
                    xRowSetPropSet->setPropertyValue( u"ActiveConnection"_ustr,  Any( xCurConnection ) );
                xRowSetPropSet->setPropertyValue( u"DataSourceName"_ustr,    Any( aCurDataSourceName ) );
                xRowSetPropSet->setPropertyValue( u"Command"_ustr,           Any( aCurDataCommand ) );
                xRowSetPropSet->setPropertyValue( u"CommandType"_ustr,       Any( nCurDataCommandType ) );
                xRowSetPropSet->setPropertyValue( u"EscapeProcessing"_ustr,  Any( bCurEscapeProcessing ) );
                xRowSetPropSet->setPropertyValue( u"ApplyFilter"_ustr,       Any( true ) );
                xRowSetPropSet->setPropertyValue( u"Filter"_ustr,            Any( aCurFilter ) );

                Reference< sdbc::XRowSet > xRowSet( xInstance, UNO_QUERY );
                if (xRowSet.is())
                    xRowSet->execute(); // build ResultSet from properties
                if( !xCurConnection.is() )
                    xCurConnection.set( xRowSetPropSet->getPropertyValue( u"ActiveConnection"_ustr ), UNO_QUERY );
                xCurResultSet = xRowSet;
                OSL_ENSURE( xCurResultSet.is(), "failed to build ResultSet" );
            }
        }
    }

    svx::ODataAccessDescriptor aDescriptor;
    aDescriptor.setDataSource(aCurDataSourceName);
    aDescriptor[ svx::DataAccessDescriptorProperty::Connection ]         <<= xCurConnection;
    aDescriptor[ svx::DataAccessDescriptorProperty::Command ]            <<= aCurDataCommand;
    aDescriptor[ svx::DataAccessDescriptorProperty::CommandType ]        <<= nCurDataCommandType;
    aDescriptor[ svx::DataAccessDescriptorProperty::EscapeProcessing ]   <<= bCurEscapeProcessing;
    aDescriptor[ svx::DataAccessDescriptorProperty::Cursor ]             <<= xCurResultSet;
    // aDescriptor[ svx::DataAccessDescriptorProperty::ColumnName ]      not used
    // aDescriptor[ svx::DataAccessDescriptorProperty::ColumnObject ]    not used
    aDescriptor[ svx::DataAccessDescriptorProperty::Selection ]          <<= aCurSelection;

    DBManagerOptions nMergeType;
    switch (nCurOutputType)
    {
        case MailMergeType::PRINTER : nMergeType = DBMGR_MERGE_PRINTER; break;
        case MailMergeType::FILE    : nMergeType = DBMGR_MERGE_FILE; break;
        case MailMergeType::MAIL    : nMergeType = DBMGR_MERGE_EMAIL; break;
        case MailMergeType::SHELL   : nMergeType = DBMGR_MERGE_SHELL; break;
        default:
            throw IllegalArgumentException(u"Invalid value of property: OutputType"_ustr, getXWeak(), 0 );
    }

    SwWrtShell &rSh = pView->GetWrtShell();
    SwDBManager* pMgr = rSh.GetDBManager();
    //force layout creation
    rSh.CalcLayout();
    OSL_ENSURE( pMgr, "database manager missing" );
    m_pMgr = pMgr;

    SwMergeDescriptor aMergeDesc( nMergeType, rSh, aDescriptor );

    std::unique_ptr< SwMailMergeConfigItem > pMMConfigItem;
    uno::Reference< mail::XMailService > xInService;
    switch (nCurOutputType)
    {
    case MailMergeType::PRINTER:
        {
            // #i25686# printing should not be done asynchronously to prevent dangling offices
            // when mail merge is called as command line macro
            aMergeDesc.aPrintOptions = m_aPrintSettings;
            aMergeDesc.bCreateSingleFile = true;
        }
        break;
    case MailMergeType::SHELL:
        aMergeDesc.bCreateSingleFile = true;
        pMMConfigItem.reset(new SwMailMergeConfigItem);
        aMergeDesc.pMailMergeConfigItem = pMMConfigItem.get();
        break;
    case MailMergeType::FILE:
        {
            INetURLObject aURLObj;
            aURLObj.SetSmartProtocol( INetProtocol::File );

            if (!aCurDocumentURL.isEmpty())
            {
                // if OutputURL or FileNamePrefix are missing get
                // them from DocumentURL
                aURLObj.SetSmartURL( aCurDocumentURL );
                if (aCurFileNamePrefix.isEmpty())
                    aCurFileNamePrefix = aURLObj.GetBase(); // filename without extension
                if (aCurOutputURL.isEmpty())
                {
                    aURLObj.removeSegment();
                    aCurOutputURL = aURLObj.GetMainURL( INetURLObject::DecodeMechanism::ToIUri );
                }
            }
            else    // default empty document without URL
            {
                if (aCurOutputURL.isEmpty())
                    throw RuntimeException(u"OutputURL is not set and can not be obtained."_ustr, getXWeak() );
            }

            aURLObj.SetSmartURL( aCurOutputURL );
            OUString aPath = aURLObj.GetMainURL( INetURLObject::DecodeMechanism::ToIUri );

            static constexpr OUString aDelim( u"/"_ustr );
            if (!aPath.isEmpty() && !aPath.endsWith(aDelim))
                aPath += aDelim;
            if (bCurFileNameFromColumn)
                aMergeDesc.sDBcolumn = aCurFileNamePrefix;
            else
            {
                aPath += aCurFileNamePrefix;
            }

            aMergeDesc.sPrefix = aPath;
            aMergeDesc.sSaveToFilter = m_sSaveFilter;
            aMergeDesc.sSaveToFilterOptions = m_sSaveFilterOptions;
            aMergeDesc.aSaveToFilterData = m_aSaveFilterData;
            aMergeDesc.bCreateSingleFile = m_bSaveAsSingleFile;
        }
        break;
    case MailMergeType::MAIL:
            {
                aMergeDesc.sDBcolumn = m_sAddressFromColumn;
                if(m_sAddressFromColumn.isEmpty())
                    throw RuntimeException(u"Mail address column not set."_ustr, getXWeak() );
                aMergeDesc.sSaveToFilter     = m_sAttachmentFilter;
                aMergeDesc.sSubject          = m_sSubject;
                aMergeDesc.sMailBody         = m_sMailBody;
                aMergeDesc.sAttachmentName   = m_sAttachmentName;
                aMergeDesc.aCopiesTo         = m_aCopiesTo;
                aMergeDesc.aBlindCopiesTo    = m_aBlindCopiesTo;
                aMergeDesc.bSendAsHTML       = m_bSendAsHTML;
                aMergeDesc.bSendAsAttachment = m_bSendAsAttachment;

                aMergeDesc.bCreateSingleFile = false;
                pMMConfigItem.reset(new SwMailMergeConfigItem);
                aMergeDesc.pMailMergeConfigItem = pMMConfigItem.get();
                aMergeDesc.xSmtpServer = SwMailMergeHelper::ConnectToSmtpServer(
                        *pMMConfigItem,
                        xInService,
                        m_sInServerPassword, m_sOutServerPassword );
                if( !aMergeDesc.xSmtpServer.is() || !aMergeDesc.xSmtpServer->isConnected())
                    throw RuntimeException(u"Failed to connect to mail server."_ustr, getXWeak() );
            }
        break;
    }

    // save document with temporary filename
    std::shared_ptr<const SfxFilter> pSfxFlt = SwIoSystem::GetFilterOfFormat(
            FILTER_XML,
            SwDocShell::Factory().GetFilterContainer() );
    OUString aExtension(comphelper::string::stripStart(pSfxFlt->GetDefaultExtension(), '*'));
    m_aTmpFileName = utl::CreateTempURL( u"SwMM", true, aExtension );

    Reference< XStorable > xStorable( xCurModel, UNO_QUERY );
    bool bStoredAsTemporary = false;
    if ( xStorable.is() )
    {
        try
        {
            xStorable->storeAsURL( m_aTmpFileName, Sequence< PropertyValue >() );
            bStoredAsTemporary = true;
        }
        catch (const Exception&)
        {
        }
    }
    if ( !bStoredAsTemporary )
        throw RuntimeException(u"Failed to save temporary file."_ustr, getXWeak() );

    pMgr->SetMergeSilent( true );       // suppress dialogs, message boxes, etc.
    const SwXMailMerge *pOldSrc = pMgr->GetMailMergeEvtSrc();
    OSL_ENSURE( !pOldSrc || pOldSrc == this, "Ooops... different event source already set." );
    pMgr->SetMailMergeEvtSrc( this );   // launch events for listeners

    SfxGetpApp()->NotifyEvent(SfxEventHint(SfxEventHintId::SwMailMerge, SwDocShell::GetEventName(STR_SW_EVENT_MAIL_MERGE), xCurDocSh.get()));
    bool bSucc = pMgr->Merge( aMergeDesc );
    SfxGetpApp()->NotifyEvent(SfxEventHint(SfxEventHintId::SwMailMergeEnd, SwDocShell::GetEventName(STR_SW_EVENT_MAIL_MERGE_END), xCurDocSh.get()));

    pMgr->SetMailMergeEvtSrc( pOldSrc );

    if ( xCurModel.get() != m_xModel.get() )
    {   // in case it was a temporary model -> close it, and delete the file
        DeleteTmpFile_Impl( xCurModel, xCurDocSh, m_aTmpFileName );
        m_aTmpFileName.clear();
    }
    // (in case it wasn't a temporary model, it will be closed in the dtor, at the latest)

    if (!bSucc)
        throw Exception(u"Mail merge failed. Sorry, no further information available."_ustr, getXWeak() );

    //de-initialize services
    if(xInService.is() && xInService->isConnected())
        xInService->disconnect();
    if(aMergeDesc.xSmtpServer.is() && aMergeDesc.xSmtpServer->isConnected())
        aMergeDesc.xSmtpServer->disconnect();

    if (DBMGR_MERGE_SHELL == nMergeType)
    {
        return Any(aMergeDesc.pMailMergeConfigItem->GetTargetView()->GetDocShell()->GetModel());
    }
    else
        return Any( true );
}

void SAL_CALL SwXMailMerge::cancel()
{
    // Cancel may be called from a second thread, so this protects from m_pMgr
    /// cleanup in the execute function.
    osl::MutexGuard aMgrGuard( GetMailMergeMutex() );
    if (m_pMgr)
        m_pMgr->MergeCancel();
}

void SwXMailMerge::LaunchMailMergeEvent( const MailMergeEvent &rEvt ) const
{
    comphelper::OInterfaceIteratorHelper2 aIt( const_cast<SwXMailMerge *>(this)->m_aMergeListeners );
    while (aIt.hasMoreElements())
    {
        static_cast< XMailMergeListener* >( aIt.next() )->notifyMailMergeEvent( rEvt );
    }
}

void SwXMailMerge::launchEvent( const PropertyChangeEvent &rEvt ) const
{
    comphelper::OInterfaceContainerHelper3<XPropertyChangeListener> *pContainer =
            m_aPropListeners.getContainer( rEvt.PropertyHandle );
    if (pContainer)
    {
        pContainer->notifyEach( &XPropertyChangeListener::propertyChange, rEvt );
    }
}

uno::Reference< beans::XPropertySetInfo > SAL_CALL SwXMailMerge::getPropertySetInfo(  )
{
    SolarMutexGuard aGuard;
    static Reference< XPropertySetInfo > aRef = m_pPropSet->getPropertySetInfo();
    return aRef;
}

void SAL_CALL SwXMailMerge::setPropertyValue(
        const OUString& rPropertyName, const uno::Any& rValue )
{
    SolarMutexGuard aGuard;

    const SfxItemPropertyMapEntry* pCur = m_pPropSet->getPropertyMap().getByName( rPropertyName );
    if (!pCur)
        throw UnknownPropertyException(rPropertyName);
    else if (pCur->nFlags & PropertyAttribute::READONLY)
        throw PropertyVetoException();
    else
    {
        void *pData = nullptr;
        switch (pCur->nWID)
        {
            case WID_SELECTION :                pData = &m_aSelection;  break;
            case WID_RESULT_SET :               pData = &m_xResultSet;  break;
            case WID_CONNECTION :               pData = &m_xConnection;  break;
            case WID_MODEL :                    pData = &m_xModel;  break;
            case WID_DATA_SOURCE_NAME :         pData = &m_aDataSourceName;  break;
            case WID_DATA_COMMAND :             pData = &m_aDataCommand;  break;
            case WID_FILTER :                   pData = &m_aFilter;  break;
            case WID_DOCUMENT_URL :             pData = &m_aDocumentURL;  break;
            case WID_OUTPUT_URL :               pData = &m_aOutputURL;  break;
            case WID_DATA_COMMAND_TYPE :        pData = &m_nDataCommandType;  break;
            case WID_OUTPUT_TYPE :              pData = &m_nOutputType;  break;
            case WID_ESCAPE_PROCESSING :        pData = &m_bEscapeProcessing;  break;
            case WID_FILE_NAME_FROM_COLUMN :    pData = &m_bFileNameFromColumn;  break;
            case WID_FILE_NAME_PREFIX :         pData = &m_aFileNamePrefix;  break;
            case WID_MAIL_SUBJECT:              pData = &m_sSubject; break;
            case WID_ADDRESS_FROM_COLUMN:       pData = &m_sAddressFromColumn; break;
            case WID_SEND_AS_HTML:              pData = &m_bSendAsHTML; break;
            case WID_SEND_AS_ATTACHMENT:        pData = &m_bSendAsAttachment; break;
            case WID_MAIL_BODY:                 pData = &m_sMailBody; break;
            case WID_ATTACHMENT_NAME:           pData = &m_sAttachmentName; break;
            case WID_ATTACHMENT_FILTER:         pData = &m_sAttachmentFilter;break;
            case WID_PRINT_OPTIONS:             pData = &m_aPrintSettings; break;
            case WID_SAVE_AS_SINGLE_FILE:       pData = &m_bSaveAsSingleFile; break;
            case WID_SAVE_FILTER:               pData = &m_sSaveFilter; break;
            case WID_SAVE_FILTER_OPTIONS:       pData = &m_sSaveFilterOptions; break;
            case WID_SAVE_FILTER_DATA:          pData = &m_aSaveFilterData; break;
            case WID_COPIES_TO:                 pData = &m_aCopiesTo; break;
            case WID_BLIND_COPIES_TO:           pData = &m_aBlindCopiesTo;break;
            case WID_IN_SERVER_PASSWORD:        pData = &m_sInServerPassword; break;
            case WID_OUT_SERVER_PASSWORD:       pData = &m_sOutServerPassword; break;
            default :
                OSL_FAIL("unknown WID");
        }
        Any aOld( pData, pCur->aType );

        bool bChanged = false;
        bool bOK = true;
        if (aOld != rValue)
        {
            if (pData == &m_aSelection)
                bOK = rValue >>= m_aSelection;
            else if (pData == &m_xResultSet)
                bOK = rValue >>= m_xResultSet;
            else if (pData == &m_xConnection)
                bOK = rValue >>= m_xConnection;
            else if (pData == &m_xModel)
                bOK = rValue >>= m_xModel;
            else if (pData == &m_aDataSourceName)
                bOK = rValue >>= m_aDataSourceName;
            else if (pData == &m_aDataCommand)
                bOK = rValue >>= m_aDataCommand;
            else if (pData == &m_aFilter)
                bOK = rValue >>= m_aFilter;
            else if (pData == &m_aDocumentURL)
            {
                OUString aText;
                bOK = rValue >>= aText;
                if (!aText.isEmpty()
                    && !LoadFromURL_impl( m_xModel, m_xDocSh, aText, true ))
                    throw RuntimeException("Failed to create document from URL: " + aText, getXWeak() );
                m_aDocumentURL = aText;
            }
            else if (pData == &m_aOutputURL)
            {
                OUString aText;
                bOK = rValue >>= aText;
                if (!aText.isEmpty())
                {
                    if (!UCB_IsDirectory(aText))
                        throw IllegalArgumentException("URL does not point to a directory: " + aText, getXWeak(), 0 );
                    if (UCB_IsReadOnlyFileName(aText))
                        throw IllegalArgumentException("URL is read-only: " + aText, getXWeak(), 0 );
                }
                m_aOutputURL = aText;
            }
            else if (pData == &m_nDataCommandType)
                bOK = rValue >>= m_nDataCommandType;
            else if (pData == &m_nOutputType)
                bOK = rValue >>= m_nOutputType;
            else if (pData == &m_bEscapeProcessing)
                bOK = rValue >>= m_bEscapeProcessing;
            else if (pData == &m_bFileNameFromColumn)
                bOK = rValue >>= m_bFileNameFromColumn;
            else if (pData == &m_aFileNamePrefix)
                bOK = rValue >>= m_aFileNamePrefix;
            else if (pData == &m_sSubject)
                bOK = rValue >>= m_sSubject;
            else if (pData == &m_sAddressFromColumn)
                bOK = rValue >>= m_sAddressFromColumn;
            else if (pData == &m_bSendAsHTML)
                bOK = rValue >>= m_bSendAsHTML;
            else if (pData == &m_bSendAsAttachment)
                bOK = rValue >>= m_bSendAsAttachment;
            else if (pData == &m_sMailBody)
                bOK = rValue >>= m_sMailBody;
            else if (pData == &m_sAttachmentName)
                bOK = rValue >>= m_sAttachmentName;
            else if (pData == &m_sAttachmentFilter)
                bOK = rValue >>= m_sAttachmentFilter;
            else if (pData == &m_aPrintSettings)
                bOK = rValue >>= m_aPrintSettings;
            else if (pData == &m_bSaveAsSingleFile)
                bOK = rValue >>= m_bSaveAsSingleFile;
            else if (pData == &m_sSaveFilter)
                bOK = rValue >>= m_sSaveFilter;
            else if (pData == &m_sSaveFilterOptions)
                bOK = rValue >>= m_sSaveFilterOptions;
            else if (pData == &m_aSaveFilterData)
                bOK = rValue >>= m_aSaveFilterData;
            else if (pData == &m_aCopiesTo)
                bOK = rValue >>= m_aCopiesTo;
            else if (pData == &m_aBlindCopiesTo)
                bOK = rValue >>= m_aBlindCopiesTo;
            else if(pData == &m_sInServerPassword)
                bOK = rValue >>= m_sInServerPassword;
            else if(pData == &m_sOutServerPassword)
                bOK = rValue >>= m_sOutServerPassword;
            else {
                OSL_FAIL("invalid pointer" );
            }
            OSL_ENSURE( bOK, "set value failed" );
            bChanged = true;
        }
        if (!bOK)
            throw IllegalArgumentException("Property type mismatch or property not set: " + rPropertyName, getXWeak(), 0 );

        if (bChanged)
        {
            PropertyChangeEvent aChgEvt( static_cast<XPropertySet *>(this), rPropertyName,
                    false, pCur->nWID, aOld, rValue );
            launchEvent( aChgEvt );
        }
    }
}

uno::Any SAL_CALL SwXMailMerge::getPropertyValue(
        const OUString& rPropertyName )
{
    SolarMutexGuard aGuard;

    Any aRet;

    const SfxItemPropertyMapEntry* pCur = m_pPropSet->getPropertyMap().getByName( rPropertyName );
    if (!pCur)
        throw UnknownPropertyException(rPropertyName);

    switch (pCur->nWID)
    {
        case WID_SELECTION :                aRet <<= m_aSelection;  break;
        case WID_RESULT_SET :               aRet <<= m_xResultSet;  break;
        case WID_CONNECTION :               aRet <<= m_xConnection;  break;
        case WID_MODEL :                    aRet <<= m_xModel;  break;
        case WID_DATA_SOURCE_NAME :         aRet <<= m_aDataSourceName;  break;
        case WID_DATA_COMMAND :             aRet <<= m_aDataCommand;  break;
        case WID_FILTER :                   aRet <<= m_aFilter;  break;
        case WID_DOCUMENT_URL :             aRet <<= m_aDocumentURL;  break;
        case WID_OUTPUT_URL :               aRet <<= m_aOutputURL;  break;
        case WID_DATA_COMMAND_TYPE :        aRet <<= m_nDataCommandType;  break;
        case WID_OUTPUT_TYPE :              aRet <<= m_nOutputType;  break;
        case WID_ESCAPE_PROCESSING :        aRet <<= m_bEscapeProcessing;  break;
        case WID_FILE_NAME_FROM_COLUMN :    aRet <<= m_bFileNameFromColumn;  break;
        case WID_FILE_NAME_PREFIX :         aRet <<= m_aFileNamePrefix;  break;
        case WID_MAIL_SUBJECT:              aRet <<= m_sSubject; break;
        case WID_ADDRESS_FROM_COLUMN:       aRet <<= m_sAddressFromColumn; break;
        case WID_SEND_AS_HTML:              aRet <<= m_bSendAsHTML; break;
        case WID_SEND_AS_ATTACHMENT:        aRet <<= m_bSendAsAttachment; break;
        case WID_MAIL_BODY:                 aRet <<= m_sMailBody; break;
        case WID_ATTACHMENT_NAME:           aRet <<= m_sAttachmentName; break;
        case WID_ATTACHMENT_FILTER:         aRet <<= m_sAttachmentFilter;break;
        case WID_PRINT_OPTIONS:             aRet <<= m_aPrintSettings; break;
        case WID_SAVE_AS_SINGLE_FILE:       aRet <<= m_bSaveAsSingleFile; break;
        case WID_SAVE_FILTER:               aRet <<= m_sSaveFilter; break;
        case WID_SAVE_FILTER_OPTIONS:       aRet <<= m_sSaveFilterOptions; break;
        case WID_SAVE_FILTER_DATA:          aRet <<= m_aSaveFilterData; break;
        case WID_COPIES_TO:                 aRet <<= m_aCopiesTo; break;
        case WID_BLIND_COPIES_TO:           aRet <<= m_aBlindCopiesTo;break;
        case WID_IN_SERVER_PASSWORD:        aRet <<= m_sInServerPassword; break;
        case WID_OUT_SERVER_PASSWORD:       aRet <<= m_sOutServerPassword; break;
        default :
            OSL_FAIL("unknown WID");
    }

    return aRet;
}

void SAL_CALL SwXMailMerge::addPropertyChangeListener(
        const OUString& rPropertyName,
        const uno::Reference< beans::XPropertyChangeListener >& rListener )
{
    SolarMutexGuard aGuard;
    if (!m_bDisposing && rListener.is())
    {
        const SfxItemPropertyMapEntry* pCur = m_pPropSet->getPropertyMap().getByName( rPropertyName );
        if (!pCur)
            throw UnknownPropertyException(rPropertyName);
        m_aPropListeners.addInterface( pCur->nWID, rListener );
    }
}

void SAL_CALL SwXMailMerge::removePropertyChangeListener(
        const OUString& rPropertyName,
        const uno::Reference< beans::XPropertyChangeListener >& rListener )
{
    SolarMutexGuard aGuard;
    if (!m_bDisposing && rListener.is())
    {
        const SfxItemPropertyMapEntry* pCur = m_pPropSet->getPropertyMap().getByName( rPropertyName );
        if (!pCur)
            throw UnknownPropertyException(rPropertyName);
        m_aPropListeners.removeInterface( pCur->nWID, rListener );
    }
}

void SAL_CALL SwXMailMerge::addVetoableChangeListener(
        const OUString& /*rPropertyName*/,
        const uno::Reference< beans::XVetoableChangeListener >& /*rListener*/ )
{
    // no vetoable property, thus no support for vetoable change listeners
    OSL_FAIL("not implemented");
}

void SAL_CALL SwXMailMerge::removeVetoableChangeListener(
        const OUString& /*rPropertyName*/,
        const uno::Reference< beans::XVetoableChangeListener >& /*rListener*/ )
{
    // no vetoable property, thus no support for vetoable change listeners
    OSL_FAIL("not implemented");
}

void SAL_CALL SwXMailMerge::dispose()
{
    SolarMutexGuard aGuard;

    if (!m_bDisposing)
    {
        m_bDisposing = true;

        EventObject aEvtObj( static_cast<XPropertySet *>(this) );
        m_aEvtListeners.disposeAndClear( aEvtObj );
        m_aMergeListeners.disposeAndClear( aEvtObj );
        m_aPropListeners.disposeAndClear( aEvtObj );
    }
}

void SAL_CALL SwXMailMerge::addEventListener(
        const Reference< XEventListener >& rxListener )
{
    SolarMutexGuard aGuard;
    if (!m_bDisposing && rxListener.is())
        m_aEvtListeners.addInterface( rxListener );
}

void SAL_CALL SwXMailMerge::removeEventListener(
        const Reference< XEventListener >& rxListener )
{
    SolarMutexGuard aGuard;
    if (!m_bDisposing && rxListener.is())
        m_aEvtListeners.removeInterface( rxListener );
}

void SAL_CALL SwXMailMerge::addMailMergeEventListener(
        const uno::Reference< XMailMergeListener >& rxListener )
{
    SolarMutexGuard aGuard;
    if (!m_bDisposing && rxListener.is())
        m_aMergeListeners.addInterface( rxListener );
}

void SAL_CALL SwXMailMerge::removeMailMergeEventListener(
        const uno::Reference< XMailMergeListener >& rxListener )
{
    SolarMutexGuard aGuard;
    if (!m_bDisposing && rxListener.is())
        m_aMergeListeners.removeInterface( rxListener );
}

OUString SAL_CALL SwXMailMerge::getImplementationName()
{
    return u"SwXMailMerge"_ustr;
}

sal_Bool SAL_CALL SwXMailMerge::supportsService( const OUString& rServiceName )
{
    return cppu::supportsService(this, rServiceName);
}

uno::Sequence< OUString > SAL_CALL SwXMailMerge::getSupportedServiceNames()
{
    return { u"com.sun.star.text.MailMerge"_ustr, u"com.sun.star.sdb.DataAccessDescriptor"_ustr };
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
