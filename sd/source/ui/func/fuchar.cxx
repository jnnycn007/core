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

#include <fuchar.hxx>
#include <sfx2/viewfrm.hxx>
#include <sfx2/sfxdlg.hxx>

#include <svx/svxids.hrc>
#include <editeng/eeitem.hxx>
#include <editeng/colritem.hxx>
#include <editeng/brushitem.hxx>
#include <sfx2/bindings.hxx>
#include <sfx2/request.hxx>
#include <View.hxx>
#include <drawdoc.hxx>
#include <ViewShell.hxx>
#include <DrawDocShell.hxx>
#include <sdabstdlg.hxx>

namespace sd {


FuChar::FuChar (
    ViewShell& rViewSh,
    ::sd::Window* pWin,
    ::sd::View* pView,
    SdDrawDocument& rDoc,
    SfxRequest& rReq)
    : FuPoor(rViewSh, pWin, pView, rDoc, rReq)
{
}

rtl::Reference<FuPoor> FuChar::Create( ViewShell& rViewSh, ::sd::Window* pWin, ::sd::View* pView, SdDrawDocument& rDoc, SfxRequest& rReq )
{
    rtl::Reference<FuPoor> xFunc( new FuChar( rViewSh, pWin, pView, rDoc, rReq ) );
    xFunc->DoExecute(rReq);
    return xFunc;
}

void FuChar::DoExecute( SfxRequest& rReq )
{
    const SfxItemSet* pArgs = rReq.GetArgs();

    if( !pArgs )
    {
        SfxItemSet aEditAttr( mrDoc.GetPool() );
        mpView->GetAttributes( aEditAttr );

        SfxItemSetFixed<XATTR_FILLSTYLE, XATTR_FILLCOLOR, EE_ITEMS_START, EE_ITEMS_END> aNewAttr(mrViewShell.GetPool());
        aNewAttr.Put( aEditAttr, false );

        SdAbstractDialogFactory* pFact = SdAbstractDialogFactory::Create();
        ScopedVclPtr<SfxAbstractTabDialog> pDlg( pFact->CreateSdTabCharDialog(mrViewShell.GetFrameWeld(), &aNewAttr, mrDoc.GetDocSh() ) );
        if (rReq.GetSlot() == SID_CHAR_DLG_EFFECT)
        {
            pDlg->SetCurPageId(u"fonteffects"_ustr);
        }

        sal_uInt16 nResult = pDlg->Execute();

        if( nResult != RET_OK )
            return;

        const SfxItemSet* pOutputSet = pDlg->GetOutputItemSet();
        SfxItemSet aOtherSet( *pOutputSet );

        // and now the reverse process
        const SvxBrushItem* pBrushItem = aOtherSet.GetItem<SvxBrushItem>( SID_ATTR_BRUSH_CHAR );

        if ( pBrushItem )
        {
            SvxColorItem aBackColorItem( pBrushItem->GetColor(), EE_CHAR_BKGCOLOR );
            aOtherSet.ClearItem( SID_ATTR_BRUSH_CHAR );
            aOtherSet.Put( aBackColorItem );
        }

        rReq.Done( aOtherSet );
        pArgs = rReq.GetArgs();
    }
    mpView->SetAttributes(*pArgs);

    // invalidate the Slots which are in DrTxtObjBar
    static const sal_uInt16 SidArray[] = {
                    SID_ATTR_CHAR_FONT,
                    SID_ATTR_CHAR_POSTURE,
                    SID_ATTR_CHAR_WEIGHT,
                    SID_ATTR_CHAR_SHADOWED,
                    SID_ATTR_CHAR_STRIKEOUT,
                    SID_ATTR_CHAR_UNDERLINE,
                    SID_ATTR_CHAR_FONTHEIGHT,
                    SID_ATTR_CHAR_COLOR,
                    SID_ATTR_CHAR_KERNING,
                    SID_ATTR_CHAR_CASEMAP,
                    SID_SET_SUPER_SCRIPT,
                    SID_SET_SUB_SCRIPT,
                    SID_ATTR_CHAR_BACK_COLOR,
                    0 };

    mrViewShell.GetViewFrame()->GetBindings().Invalidate( SidArray );

    if( mrDoc.GetOnlineSpell() )
    {
        if( SfxItemState::SET == pArgs->GetItemState(EE_CHAR_LANGUAGE, false ) ||
            SfxItemState::SET == pArgs->GetItemState(EE_CHAR_LANGUAGE_CJK, false ) ||
            SfxItemState::SET == pArgs->GetItemState(EE_CHAR_LANGUAGE_CTL, false ) )
        {
            mrDoc.StopOnlineSpelling();
            mrDoc.StartOnlineSpelling();
        }
    }
}

void FuChar::Activate()
{
}

void FuChar::Deactivate()
{
}

} // end of namespace sd

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
