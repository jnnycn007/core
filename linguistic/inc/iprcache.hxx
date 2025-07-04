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

#include <cppuhelper/implbase.hxx>

#include <com/sun/star/uno/Reference.h>
#include <com/sun/star/beans/XPropertyChangeListener.hpp>
#include <com/sun/star/linguistic2/XDictionaryListEventListener.hpp>
#include <com/sun/star/linguistic2/XSearchableDictionaryList.hpp>
#include <com/sun/star/linguistic2/XLinguProperties.hpp>

#include <rtl/ref.hxx>
#include <i18nlangtag/lang.h>

#include <set>
#include <map>

namespace linguistic
{

class SpellCache;

class FlushListener final :
    public cppu::WeakImplHelper
    <
        css::linguistic2::XDictionaryListEventListener,
        css::beans::XPropertyChangeListener
    >
{
    css::uno::Reference< css::linguistic2::XSearchableDictionaryList >    xDicList;
    css::uno::Reference< css::linguistic2::XLinguProperties >             xPropSet;
    SpellCache&                                                           mrSpellCache;

    FlushListener(const FlushListener &) = delete;
    FlushListener & operator = (const FlushListener &) = delete;

public:
    FlushListener( SpellCache& rFO ) : mrSpellCache(rFO) {}

    void        SetDicList( css::uno::Reference< css::linguistic2::XSearchableDictionaryList > const &rDL );
    void        SetPropSet( css::uno::Reference< css::linguistic2::XLinguProperties > const &rPS );

    //XEventListener
    virtual void SAL_CALL disposing( const css::lang::EventObject& rSource ) override;

    // XDictionaryListEventListener
    virtual void SAL_CALL processDictionaryListEvent( const css::linguistic2::DictionaryListEvent& rDicListEvent ) override;

    // XPropertyChangeListener
    virtual void SAL_CALL propertyChange( const css::beans::PropertyChangeEvent& rEvt ) override;
};


class SpellCache final
{
    rtl::Reference<FlushListener>  mxFlushLstnr;

    typedef std::set< OUString >                  WordList_t;
    typedef std::map< LanguageType, WordList_t >  LangWordList_t;
    LangWordList_t  aWordLists;

    SpellCache(const SpellCache &) = delete;
    SpellCache & operator = (const SpellCache &) = delete;

public:
    SpellCache();
    ~SpellCache();

    // called from FlushListener
    void    Flush();

    void    AddWord( const OUString& rWord, LanguageType nLang );
    bool    CheckWord( const OUString& rWord, LanguageType nLang );
};


}   // namespace linguistic

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
