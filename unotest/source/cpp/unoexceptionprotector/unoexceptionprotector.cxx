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

#include <cstdlib>
#include <string>
#include <string_view>

#include <com/sun/star/uno/Any.hxx>
#include <com/sun/star/uno/Exception.hpp>
#include <cppuhelper/exc_hlp.hxx>
#include <cppunit/Message.h>
#include <osl/thread.h>
#include <rtl/string.hxx>
#include <rtl/ustring.hxx>
#include <sal/types.h>

#include <cppunit/Protector.h>

namespace {

// Best effort conversion:
std::string convert(std::u16string_view s16) {
    OString s8(OUStringToOString(s16, osl_getThreadTextEncoding()));
    static_assert(sizeof (sal_Int32) <= sizeof (std::string::size_type), "got to be at least equal");
        // ensure following cast is legitimate
    return std::string(s8);
}

class Prot : public CppUnit::Protector
{
public:
    Prot() {}

    Prot(const Prot&) = delete;
    Prot& operator=(const Prot&) = delete;

    virtual bool protect(
        CppUnit::Functor const & functor,
        CppUnit::ProtectorContext const & context) override;
};

bool Prot::protect(
    CppUnit::Functor const & functor, CppUnit::ProtectorContext const & context)
{
    try {
        return functor();
    } catch (const css::uno::Exception &e) {
        css::uno::Any a(cppu::getCaughtException());
        reportError(
            context,
            CppUnit::Message(
                "An uncaught exception of type " + convert(a.getValueTypeName()),
                convert(e.Message)));
    }
    return false;
}

}

extern "C" SAL_DLLPUBLIC_EXPORT CppUnit::Protector *
unoexceptionprotector() {
    return std::getenv("CPPUNIT_PROPAGATE_EXCEPTIONS") == nullptr ? new Prot : nullptr;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
