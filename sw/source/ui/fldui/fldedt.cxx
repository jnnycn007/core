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

#include <config_features.h>
#include <config_fuzzers.h>

#include <sfx2/basedlgs.hxx>
#include <sfx2/viewfrm.hxx>
#include <svx/optgenrl.hxx>
#include <docufld.hxx>
#include <expfld.hxx>
#include <view.hxx>
#include <wrtsh.hxx>
#include "flddb.hxx"
#include "flddinf.hxx"
#include "fldvar.hxx"
#include "flddok.hxx"
#include "fldfunc.hxx"
#include "fldref.hxx"
#include <fldedt.hxx>

#include <cmdid.h>
#include <swabstdlg.hxx>
#include <comphelper/dispatchcommand.hxx>

#include <com/sun/star/beans/XPropertySet.hpp>
#include <com/sun/star/document/XDocumentProperties.hpp>
#include <com/sun/star/document/XDocumentPropertiesSupplier.hpp>
#include <com/sun/star/frame/XModel.hpp>
#include <memory>
#include <swuiexp.hxx>

void SwFieldEditDlg::EnsureSelection(SwField *pCurField, SwFieldMgr &rMgr)
{
    if (m_pSh->CursorInsideInputField())
    {
        // move cursor to start of Input Field
        SwInputField* pInputField = dynamic_cast<SwInputField*>(pCurField);
        if (pInputField && pInputField->GetFormatField())
        {
            m_pSh->GotoField( *(pInputField->GetFormatField()) );
        }
        else
        {
            SwSetExpField *const pSetField(dynamic_cast<SwSetExpField*>(pCurField));
            if (pSetField)
            {
                assert(pSetField->GetFormatField());
                m_pSh->GotoField( *(pSetField->GetFormatField()) );
            }
            else
            {
                assert(!"what input field is this");
            }
        }
    }

    /* Only create selection if there is none already.
       Normalize PaM instead of swapping. */
    if (!m_pSh->HasSelection())
    {
        SwShellCursor* pCursor = m_pSh->getShellCursor(true);
        SwPosition aOrigPos(*pCursor->GetPoint());

        //After this attempt it is possible that rMgr.GetCurField() != pCurField if
        //the field was in e.g. a zero height portion and so invisible in which
        //case it will be skipped over
        m_pSh->Right(SwCursorSkipMode::Chars, true, 1, false );
        //So (fdo#50640) if it didn't work then reposition back to the original
        //location where the field was
        SwField *pRealCurField = rMgr.GetCurField();
        bool bSelectionFailed = pCurField != pRealCurField;
        if (bSelectionFailed)
        {
            pCursor->DeleteMark();
            *pCursor->GetPoint() = std::move(aOrigPos);
        }
    }

    m_pSh->NormalizePam();

    assert(pCurField == rMgr.GetCurField());
}

SwFieldEditDlg::SwFieldEditDlg(SwView const & rVw)
    : SfxSingleTabDialogController(rVw.GetViewFrame().GetFrameWeld(), nullptr,
        u"modules/swriter/ui/editfielddialog.ui"_ustr, u"EditFieldDialog"_ustr)
    , m_pSh(rVw.GetWrtShellPtr())
    , m_xPrevBT(m_xBuilder->weld_button(u"prev"_ustr))
    , m_xNextBT(m_xBuilder->weld_button(u"next"_ustr))
    , m_xAddressBT(m_xBuilder->weld_button(u"edit"_ustr))
{
    SwFieldMgr aMgr(m_pSh);

    SwField *pCurField = aMgr.GetCurField();
    if (!pCurField)
        return;

    SwViewShell::SetCareDialog(m_xDialog);

    EnsureSelection(pCurField, aMgr);

    sal_uInt16 nGroup = SwFieldMgr::GetGroup(pCurField->GetTypeId(), pCurField->GetUntypedSubType());

    CreatePage(nGroup);

    GetOKButton().connect_clicked(LINK(this, SwFieldEditDlg, OKHdl));

    m_xPrevBT->connect_clicked(LINK(this, SwFieldEditDlg, NextPrevHdl));
    m_xNextBT->connect_clicked(LINK(this, SwFieldEditDlg, NextPrevHdl));

    m_xAddressBT->connect_clicked(LINK(this, SwFieldEditDlg, AddressHdl));

    Init();
}

// initialise controls
void SwFieldEditDlg::Init()
{
    SwFieldPage* pTabPage = static_cast<SwFieldPage*>(GetTabPage());
    if (pTabPage)
    {
        SwFieldMgr& rMgr = pTabPage->GetFieldMgr();

        SwField *pCurField = rMgr.GetCurField();

        if(!pCurField)
            return;

        // Traveling only when more than one field
        m_pSh->StartAction();
        m_pSh->ClearMark();
        m_pSh->CreateCursor();

        bool bMove = rMgr.GoNext();
        if( bMove )
            rMgr.GoPrev();
        m_xNextBT->set_sensitive(bMove);

        bMove = rMgr.GoPrev();
        if( bMove )
            rMgr.GoNext();
        m_xPrevBT->set_sensitive( bMove );
        {
            const SwFieldTypesEnum aId(pCurField->GetTypeId());
            if ( (aId == SwFieldTypesEnum::Author) ||
                 ((aId == SwFieldTypesEnum::DocumentInfo) && (static_cast<const SwDocInfoField*>(pCurField)->GetSubType() != (SwDocInfoSubType::Create | SwDocInfoSubType::SubAuthor)))) // except DocumentInfo > Author
                m_xAddressBT->set_visible(true);
            else
                m_xAddressBT->set_visible(false);
        }

        m_pSh->DestroyCursor();
        m_pSh->EndAction();
    }

    GetOKButton().set_sensitive(!m_pSh->IsReadOnlyAvailable() ||
                                !m_pSh->HasReadonlySel());
}

SfxTabPage* SwFieldEditDlg::CreatePage(sal_uInt16 nGroup)
{
    // create TabPage
    std::unique_ptr<SfxTabPage> xTabPage;

    switch (nGroup)
    {
        case GRP_DOC:
            xTabPage = SwFieldDokPage::Create(get_content_area(), this, nullptr);
            break;
        case GRP_FKT:
            xTabPage = SwFieldFuncPage::Create(get_content_area(), this, nullptr);
            break;
        case GRP_REF:
            xTabPage = SwFieldRefPage::Create(get_content_area(), this, nullptr);
            break;
        case GRP_REG:
            if (SfxObjectShell* pDocSh = SfxObjectShell::Current())
            {
                auto pSet = new SfxItemSetFixed<FN_FIELD_DIALOG_DOC_PROPS, FN_FIELD_DIALOG_DOC_PROPS>( pDocSh->GetPool() );
                using namespace ::com::sun::star;
                uno::Reference<document::XDocumentPropertiesSupplier> xDPS(
                    pDocSh->GetModel(), uno::UNO_QUERY_THROW);
                uno::Reference<document::XDocumentProperties> xDocProps
                    = xDPS->getDocumentProperties();
                uno::Reference< beans::XPropertySet > xUDProps(
                    xDocProps->getUserDefinedProperties(),
                    uno::UNO_QUERY_THROW);
                pSet->Put( SfxUnoAnyItem( FN_FIELD_DIALOG_DOC_PROPS, uno::Any(xUDProps) ) );
                xTabPage = SwFieldDokInfPage::Create(get_content_area(), this, pSet);
            }
            break;
#if HAVE_FEATURE_DBCONNECTIVITY && !ENABLE_FUZZERS
        case GRP_DB:
            xTabPage = SwFieldDBPage::Create(get_content_area(), this, nullptr);
            static_cast<SwFieldDBPage*>(xTabPage.get())->SetWrtShell(*m_pSh);
            break;
#endif
        case GRP_VAR:
            xTabPage = SwFieldVarPage::Create(get_content_area(), this, nullptr);
            break;

    }

    assert(xTabPage);

    static_cast<SwFieldPage*>(xTabPage.get())->SetWrtShell(m_pSh);
    SetTabPage(std::move(xTabPage));

    return GetTabPage();
}

void SwFieldEditDlg::ImplDestroy()
{
    SwViewShell::SetCareDialog(nullptr);
    m_pSh->EnterStdMode();
}

SwFieldEditDlg::~SwFieldEditDlg()
{
    suppress_fun_call_w_exception(ImplDestroy());
}

void SwFieldEditDlg::EnableInsert(bool bEnable)
{
    if( bEnable && m_pSh->IsReadOnlyAvailable() && m_pSh->HasReadonlySel() )
        bEnable = false;
    GetOKButton().set_sensitive(bEnable);
}

void SwFieldEditDlg::InsertHdl()
{
    GetOKButton().clicked();
}

// kick off changing of the field
IMPL_LINK_NOARG(SwFieldEditDlg, OKHdl, weld::Button&, void)
{
    if (GetOKButton().get_sensitive())
    {
        SfxTabPage* pTabPage = GetTabPage();
        if (pTabPage)
            pTabPage->FillItemSet(nullptr);
        m_xDialog->response(RET_OK);
    }
}

// Traveling between fields of the same type
IMPL_LINK(SwFieldEditDlg, NextPrevHdl, weld::Button&, rButton, void)
{
    bool bNext = &rButton == m_xNextBT.get();

    m_pSh->EnterStdMode();

    SwFieldType *pOldTyp = nullptr;
    SwFieldPage* pTabPage = static_cast<SwFieldPage*>(GetTabPage());

    //#112462# FillItemSet may delete the current field
    //that's why it has to be called before accessing the current field
    if (GetOKButton().get_sensitive())
        pTabPage->FillItemSet(nullptr);

    SwFieldMgr& rMgr = pTabPage->GetFieldMgr();
    SwField *pCurField = rMgr.GetCurField();
    if (pCurField->GetTypeId() == SwFieldTypesEnum::Database)
        pOldTyp = pCurField->GetTyp();

    rMgr.GoNextPrev( bNext, pOldTyp );
    pCurField = rMgr.GetCurField();

    sal_uInt16 nGroup = SwFieldMgr::GetGroup(pCurField->GetTypeId(), pCurField->GetUntypedSubType());

    if (nGroup != pTabPage->GetGroup())
        pTabPage = static_cast<SwFieldPage*>(CreatePage(nGroup));

    pTabPage->EditNewField();

    Init();
    EnsureSelection(pCurField, rMgr);
}

IMPL_LINK_NOARG(SwFieldEditDlg, AddressHdl, weld::Button&, void)
{
    SwFieldPage* pTabPage = static_cast<SwFieldPage*>(GetTabPage());
    SwFieldMgr& rMgr = pTabPage->GetFieldMgr();
    SwField *pCurField = rMgr.GetCurField();

    if (pCurField->GetTypeId() == SwFieldTypesEnum::DocumentInfo)
        comphelper::dispatchCommand(u".uno:SetDocumentProperties"_ustr, {});
    else
    {
        SfxItemSetFixed<SID_FIELD_GRABFOCUS, SID_FIELD_GRABFOCUS> aSet( m_pSh->GetAttrPool() );

        EditPosition nEditPos = EditPosition::UNKNOWN;

        auto pExtUserField = static_cast<const SwExtUserField*>(pCurField);
        switch(pExtUserField->GetSubType())
        {
            case SwExtUserSubType::Firstname:  nEditPos = EditPosition::FIRSTNAME;  break;
            case SwExtUserSubType::Name:       nEditPos = EditPosition::LASTNAME;   break;
            case SwExtUserSubType::Shortcut:   nEditPos = EditPosition::SHORTNAME;  break;
            case SwExtUserSubType::Company:    nEditPos = EditPosition::COMPANY;    break;
            case SwExtUserSubType::Street:     nEditPos = EditPosition::STREET;     break;
            case SwExtUserSubType::Title:      nEditPos = EditPosition::TITLE;      break;
            case SwExtUserSubType::Position:   nEditPos = EditPosition::POSITION;   break;
            case SwExtUserSubType::PhonePrivate:nEditPos = EditPosition::TELPRIV;  break;
            case SwExtUserSubType::PhoneCompany:nEditPos = EditPosition::TELCOMPANY;   break;
            case SwExtUserSubType::Fax:        nEditPos = EditPosition::FAX;        break;
            case SwExtUserSubType::Email:      nEditPos = EditPosition::EMAIL;      break;
            case SwExtUserSubType::Country:    nEditPos = EditPosition::COUNTRY;    break;
            case SwExtUserSubType::Zip:        nEditPos = EditPosition::PLZ;        break;
            case SwExtUserSubType::City:       nEditPos = EditPosition::CITY;       break;
            case SwExtUserSubType::State:      nEditPos = EditPosition::STATE;      break;

            default:            nEditPos = EditPosition::UNKNOWN;    break;

        }
        aSet.Put(SfxUInt16Item(SID_FIELD_GRABFOCUS, static_cast<sal_uInt16>(nEditPos)));

        SwAbstractDialogFactory& rFact = swui::GetFactory();
        ScopedVclPtr<SfxAbstractDialog> pDlg(rFact.CreateSwAddressAbstractDlg(m_xDialog.get(), aSet));
        if (RET_OK == pDlg->Execute())
        {
            m_pSh->UpdateOneField(*pCurField);
        }
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
