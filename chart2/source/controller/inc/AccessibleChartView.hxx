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
#pragma once

#include "AccessibleBase.hxx"
#include "ChartWindow.hxx"
#include <cppuhelper/implbase.hxx>
#include <cppuhelper/weakref.hxx>
#include <com/sun/star/lang/XInitialization.hpp>
#include <com/sun/star/view/XSelectionChangeListener.hpp>

#include <memory>

namespace com::sun::star::accessibility { class XAccessible; }

namespace accessibility
{
class IAccessibleViewForwarder;
}

namespace chart
{
class ChartView;

class AccessibleChartView final
    : public cppu::ImplInheritanceHelper<chart::AccessibleBase, css::view::XSelectionChangeListener>
{
public:
    AccessibleChartView( SdrView* pView );
    virtual ~AccessibleChartView() override;

    AccessibleChartView() = delete;

    virtual void SAL_CALL disposing() override;

    // 0: view::XSelectionSupplier offers notifications for selection changes and access to the selection itself
    // 1: frame::XModel representing the chart model - offers access to object data
    // 2: lang::XInterface representing the normal chart view - offers access to some extra object data
    // 3: accessibility::XAccessible representing the parent accessible
    // 4: ChartWindow representing the view's window
    // all arguments are only valid until next initialization - don't keep them longer
    void initialize( ChartController& rChartController,
                     const rtl::Reference<::chart::ChartModel>& xChartModel,
                     const rtl::Reference<::chart::ChartView>& xChartView,
                     const css::uno::Reference< css::accessibility::XAccessible >& xParent,
                     ChartWindow* pNewChartWindow);
    // used to disconnect from view
    void initialize();

    // ____ view::XSelectionChangeListener ____
    virtual void SAL_CALL selectionChanged( const css::lang::EventObject& aEvent ) override;

    // ________ XEventListener ________
    virtual void SAL_CALL disposing( const css::lang::EventObject& Source ) override;

    // ________ XAccessibleContext ________
    virtual OUString SAL_CALL getAccessibleDescription() override;
    virtual css::uno::Reference< css::accessibility::XAccessible > SAL_CALL getAccessibleParent() override;
    virtual sal_Int64 SAL_CALL getAccessibleIndexInParent() override;
    virtual OUString SAL_CALL getAccessibleName() override;
    virtual sal_Int16 SAL_CALL getAccessibleRole() override;

    // OAccessible
    virtual css::awt::Rectangle implGetBounds() override;

protected:
    // ________ AccessibleChartElement ________
    virtual css::awt::Point   GetUpperLeftOnScreen() const override;

private: // methods
    /** @return the result that m_xWindow->getPosSize() _should_ return.  It
                returns (0,0) as upper left corner.  When calling
                getAccessibleParent, you get the parent's parent, which contains
                a decoration.  Thus you have an offset of (currently) (2,2)
                which isn't taken into account.
     */
    css::awt::Rectangle GetWindowPosSize() const;

private: // members
    unotools::WeakReference< ::chart::ChartController >             m_xChartController;
    unotools::WeakReference< ::chart::ChartModel >                  m_xChartModel;
    unotools::WeakReference< ChartView >                            m_xChartView;
    VclPtr<ChartWindow>                                             m_pChartWindow;
    css::uno::WeakReference< css::accessibility::XAccessible >      m_xParent;

    std::shared_ptr< ObjectHierarchy >                              m_spObjectHierarchy;
    AccessibleUniqueId                                              m_aCurrentSelectionOID;
    SdrView*                                                        m_pSdrView;
    std::unique_ptr<::accessibility::IAccessibleViewForwarder>      m_pViewForwarder;
};

} //namespace chart

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
