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

#include "PresenterPane.hxx"
#include "PresenterController.hxx"
#include "PresenterPaintManager.hxx"
#include <PresenterHelper.hxx>
#include <com/sun/star/lang/XMultiComponentFactory.hpp>

using namespace ::com::sun::star;
using namespace ::com::sun::star::uno;

namespace sdext::presenter {

//===== PresenterPane =========================================================

PresenterPane::PresenterPane (
    const Reference<XComponentContext>& rxContext,
        const ::rtl::Reference<PresenterController>& rpPresenterController)
    : PresenterPaneBase(rxContext, rpPresenterController)
{
}

PresenterPane::~PresenterPane()
{
}

//----- AbstractPane -----------------------------------------------------------------

Reference<awt::XWindow> PresenterPane::getWindow()
{
    {
        std::unique_lock l(m_aMutex);
        throwIfDisposed(l);
    }
    return mxContentWindow;
}

Reference<rendering::XCanvas> PresenterPane::getCanvas()
{
    {
        std::unique_lock l(m_aMutex);
        throwIfDisposed(l);
    }
    return mxContentCanvas;
}

//----- XWindowListener -------------------------------------------------------

void SAL_CALL PresenterPane::windowResized (const awt::WindowEvent& rEvent)
{
    PresenterPaneBase::windowResized(rEvent);

    Invalidate(maBoundingBox);

    LayoutContextWindow();
    ToTop();

    UpdateBoundingBox();
    Invalidate(maBoundingBox);
}

void SAL_CALL PresenterPane::windowMoved (const awt::WindowEvent& rEvent)
{
    PresenterPaneBase::windowMoved(rEvent);

    Invalidate(maBoundingBox);

    ToTop();

    UpdateBoundingBox();
    Invalidate(maBoundingBox);
}

void SAL_CALL PresenterPane::windowShown (const lang::EventObject& rEvent)
{
    PresenterPaneBase::windowShown(rEvent);

    ToTop();

    if (mxContentWindow.is())
    {
        LayoutContextWindow();
        mxContentWindow->setVisible(true);
    }

    UpdateBoundingBox();
    Invalidate(maBoundingBox);
}

void SAL_CALL PresenterPane::windowHidden (const lang::EventObject& rEvent)
{
    PresenterPaneBase::windowHidden(rEvent);

    if (mxContentWindow.is())
        mxContentWindow->setVisible(false);
}

//----- XPaintListener --------------------------------------------------------

void SAL_CALL PresenterPane::windowPaint (const awt::PaintEvent& rEvent)
{
    {
        std::unique_lock l(m_aMutex);
        throwIfDisposed(l);
    }

    PaintBorder(rEvent.UpdateRect);
}


void PresenterPane::CreateCanvases (
    const Reference<rendering::XSpriteCanvas>& rxParentCanvas)
{
    if ( ! mxParentWindow.is())
        return;
    if ( ! rxParentCanvas.is())
        return;

    mxBorderCanvas = sd::presenter::PresenterHelper::createSharedCanvas(
        rxParentCanvas,
        mxParentWindow,
        rxParentCanvas,
        mxParentWindow,
        mxBorderWindow);
    mxContentCanvas = sd::presenter::PresenterHelper::createSharedCanvas(
        rxParentCanvas,
        mxParentWindow,
        rxParentCanvas,
        mxParentWindow,
        mxContentWindow);

    PaintBorder(mxBorderWindow->getPosSize());
}

void PresenterPane::Invalidate (const css::awt::Rectangle& rRepaintBox)
{
    // Invalidate the parent window to be able to invalidate an area outside
    // the current window area.
    mpPresenterController->GetPaintManager()->Invalidate(mxParentWindow, rRepaintBox);
}

void PresenterPane::UpdateBoundingBox()
{
    uno::Reference<css::awt::XWindow2> xWindow(mxBorderWindow, UNO_QUERY);
    if (xWindow.is() && xWindow->isVisible())
        maBoundingBox = mxBorderWindow->getPosSize();
    else
        maBoundingBox = awt::Rectangle();
}

} // end of namespace ::sdext::presenter

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
