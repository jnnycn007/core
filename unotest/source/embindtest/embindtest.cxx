/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <sal/config.h>

#include <cstdlib>
#include <iostream>
#include <source_location>

#include <com/sun/star/beans/NamedValue.hpp>
#include <com/sun/star/lang/IllegalArgumentException.hpp>
#include <com/sun/star/lang/XServiceInfo.hpp>
#include <com/sun/star/task/XJob.hpp>
#include <com/sun/star/task/XJobExecutor.hpp>
#include <com/sun/star/uno/Any.hxx>
#include <com/sun/star/uno/Reference.hxx>
#include <com/sun/star/uno/RuntimeException.hpp>
#include <com/sun/star/uno/Sequence.hxx>
#include <com/sun/star/uno/Type.hxx>
#include <com/sun/star/uno/XInterface.hpp>
#include <cppu/unotype.hxx>
#include <cppuhelper/implbase.hxx>
#include <cppuhelper/supportsservice.hxx>
#include <cppuhelper/weak.hxx>
#include <o3tl/any.hxx>
#include <org/libreoffice/embindtest/Enum.hpp>
#include <org/libreoffice/embindtest/Exception.hpp>
#include <org/libreoffice/embindtest/Struct.hpp>
#include <org/libreoffice/embindtest/StructLong.hpp>
#include <org/libreoffice/embindtest/StructString.hpp>
#include <org/libreoffice/embindtest/Template.hpp>
#include <org/libreoffice/embindtest/Test.hpp>
#include <org/libreoffice/embindtest/XTest.hpp>
#include <rtl/ref.hxx>
#include <rtl/ustring.hxx>
#include <sal/types.h>
#include <salhelper/thread.hxx>
#include <uno/dispatcher.hxx>
#include <uno/environment.h>
#include <uno/environment.hxx>
#include <uno/mapping.hxx>
#include <vcl/svapp.hxx>

namespace com::sun::star::uno
{
class XComponentContext;
}

namespace
{
void verify(bool value, std::source_location const& location = std::source_location::current())
{
    if (!value)
    {
        std::cerr << "Verification failed in " << location.function_name() << " at "
                  << location.file_name() << ":" << location.line() << ":" << location.column()
                  << "\n";
        std::abort();
    }
}

class TestThread : public salhelper::Thread
{
public:
    TestThread()
        : Thread("embindtest")
    {
    }

    bool value = false;

private:
    void execute() override
    {
        SolarMutexGuard g;
        value = true;
    }
};

class JobExecutorThread : public salhelper::Thread
{
public:
    JobExecutorThread(css::uno::Reference<css::task::XJobExecutor> const& object)
        : Thread("jobexecutor")
        , object_(object)
    {
    }

private:
    void execute() override { object_->trigger(u"executor thread"_ustr); }

    css::uno::Reference<css::task::XJobExecutor> object_;
};

class Test
    : public cppu::WeakImplHelper<css::lang::XServiceInfo, org::libreoffice::embindtest::XTest>
{
    OUString SAL_CALL getImplementationName() override
    {
        return u"org.libreoffice.comp.embindtest.Test"_ustr;
    }

    sal_Bool SAL_CALL supportsService(OUString const& ServiceName) override
    {
        return cppu::supportsService(this, ServiceName);
    }

    css::uno::Sequence<OUString> SAL_CALL getSupportedServiceNames() override
    {
        return { u"org.libreoffice.embindtest.Test"_ustr };
    }

    sal_Bool SAL_CALL getBoolean() override { return true; }

    sal_Bool SAL_CALL isBoolean(sal_Bool value) override { return value; }

    sal_Int8 SAL_CALL getByte() override { return -12; }

    sal_Bool SAL_CALL isByte(sal_Int8 value) override { return value == -12; }

    sal_Int16 SAL_CALL getShort() override { return -1234; }

    sal_Bool SAL_CALL isShort(sal_Int16 value) override { return value == -1234; }

    sal_uInt16 SAL_CALL getUnsignedShort() override { return 54321; }

    sal_Bool SAL_CALL isUnsignedShort(sal_uInt16 value) override { return value == 54321; }

    sal_Int32 SAL_CALL getLong() override { return -123456; }

    sal_Bool SAL_CALL isLong(sal_Int32 value) override { return value == -123456; }

    sal_uInt32 SAL_CALL getUnsignedLong() override { return 3456789012; }

    sal_Bool SAL_CALL isUnsignedLong(sal_uInt32 value) override { return value == 3456789012; }

    sal_Int64 SAL_CALL getHyper() override { return -123456789; }

    sal_Bool SAL_CALL isHyper(sal_Int64 value) override { return value == -123456789; }

    sal_uInt64 SAL_CALL getUnsignedHyper() override { return 9876543210; }

    sal_Bool SAL_CALL isUnsignedHyper(sal_uInt64 value) override { return value == 9876543210; }

    float SAL_CALL getFloat() override { return -10.25; }

    sal_Bool SAL_CALL isFloat(float value) override { return value == -10.25; }

    double SAL_CALL getDouble() override { return 100.5; }

    sal_Bool SAL_CALL isDouble(double value) override { return value == 100.5; }

    sal_Unicode SAL_CALL getChar() override { return u'Ö'; }

    sal_Bool SAL_CALL isChar(sal_Unicode value) override { return value == u'Ö'; }

    OUString SAL_CALL getString() override { return u"hä"_ustr; }

    sal_Bool SAL_CALL isString(OUString const& value) override { return value == u"hä"; }

    css::uno::Type SAL_CALL getType() override { return cppu::UnoType<sal_Int32>::get(); }

    sal_Bool SAL_CALL isType(css::uno::Type const& value) override
    {
        return value == cppu::UnoType<sal_Int32>::get();
    }

    org::libreoffice::embindtest::Enum SAL_CALL getEnum() override
    {
        return org::libreoffice::embindtest::Enum_E_2;
    }

    sal_Bool SAL_CALL isEnum(org::libreoffice::embindtest::Enum value) override
    {
        return value == org::libreoffice::embindtest::Enum_E_2;
    }

    org::libreoffice::embindtest::Struct SAL_CALL getStruct() override
    {
        return { true,
                 -12,
                 -1234,
                 54321,
                 -123456,
                 3456789012,
                 -123456789,
                 9876543210,
                 -10.25,
                 100.5,
                 u'Ö',
                 u"hä"_ustr,
                 cppu::UnoType<sal_Int32>::get(),
                 css::uno::Any(sal_Int32(-123456)),
                 { u"foo"_ustr, u"barr"_ustr, u"bazzz"_ustr },
                 org::libreoffice::embindtest::Enum_E_2,
                 { -123456 },
                 { { u"foo"_ustr }, -123456, css::uno::Any(sal_Int32(-123456)), { u"barr"_ustr } },
                 static_cast<OWeakObject*>(this) };
    }

    sal_Bool SAL_CALL isStruct(org::libreoffice::embindtest::Struct const& value) override
    {
        return value
               == org::libreoffice::embindtest::Struct{ true,
                                                        -12,
                                                        -1234,
                                                        54321,
                                                        -123456,
                                                        3456789012,
                                                        -123456789,
                                                        9876543210,
                                                        -10.25,
                                                        100.5,
                                                        u'Ö',
                                                        u"hä"_ustr,
                                                        cppu::UnoType<sal_Int32>::get(),
                                                        css::uno::Any(sal_Int32(-123456)),
                                                        { u"foo"_ustr, u"barr"_ustr,
                                                          u"bazzz"_ustr },
                                                        org::libreoffice::embindtest::Enum_E_2,
                                                        { -123456 },
                                                        { { u"foo"_ustr },
                                                          -123456,
                                                          css::uno::Any(sal_Int32(-123456)),
                                                          { u"barr"_ustr } },
                                                        static_cast<OWeakObject*>(this) };
    }

    org::libreoffice::embindtest::StructLong SAL_CALL getStructLong() override
    {
        return { -123456 };
    }

    sal_Bool SAL_CALL isStructLong(org::libreoffice::embindtest::StructLong const& value) override
    {
        return value.m == -123456;
    }

    org::libreoffice::embindtest::StructString SAL_CALL getStructString() override
    {
        return { u"hä"_ustr };
    }

    sal_Bool SAL_CALL
    isStructString(org::libreoffice::embindtest::StructString const& value) override
    {
        return value.m == u"hä";
    }

    org::libreoffice::embindtest::Template<css::uno::Any,
                                           org::libreoffice::embindtest::StructString>
        SAL_CALL getTemplate() override
    {
        return { { u"foo"_ustr }, -123456, css::uno::Any(sal_Int32(-123456)), { u"barr"_ustr } };
    }

    sal_Bool SAL_CALL
    isTemplate(org::libreoffice::embindtest::Template<
               css::uno::Any, org::libreoffice::embindtest::StructString> const& value) override
    {
        return value
               == org::libreoffice::embindtest::Template<
                      css::uno::Any, org::libreoffice::embindtest::StructString>{
                      { u"foo"_ustr }, -123456, css::uno::Any(sal_Int32(-123456)), { u"barr"_ustr }
                  };
    }

    css::uno::Any SAL_CALL getAnyVoid() override { return {}; }

    sal_Bool SAL_CALL isAnyVoid(css::uno::Any const& value) override
    {
        return value.getValueType() == cppu::UnoType<void>::get();
    }

    css::uno::Any SAL_CALL getAnyBoolean() override { return css::uno::Any(true); }

    sal_Bool SAL_CALL isAnyBoolean(css::uno::Any const& value) override
    {
        return value.getValueType() == cppu::UnoType<bool>::get()
               && *o3tl::forceAccess<bool>(value);
    }

    css::uno::Any SAL_CALL getAnyByte() override { return css::uno::Any(sal_Int8(-12)); }

    sal_Bool SAL_CALL isAnyByte(css::uno::Any const& value) override
    {
        return value.getValueType() == cppu::UnoType<sal_Int8>::get()
               && *o3tl::forceAccess<sal_Int8>(value) == -12;
    }

    css::uno::Any SAL_CALL getAnyShort() override { return css::uno::Any(sal_Int16(-1234)); }

    sal_Bool SAL_CALL isAnyShort(css::uno::Any const& value) override
    {
        return value.getValueType() == cppu::UnoType<sal_Int16>::get()
               && *o3tl::forceAccess<sal_Int16>(value) == -1234;
    }

    css::uno::Any SAL_CALL getAnyUnsignedShort() override
    {
        return css::uno::Any(sal_uInt16(54321));
    }

    sal_Bool SAL_CALL isAnyUnsignedShort(css::uno::Any const& value) override
    {
        return value.getValueType() == cppu::UnoType<sal_uInt16>::get()
               && *o3tl::forceAccess<sal_uInt16>(value) == 54321;
    }

    css::uno::Any SAL_CALL getAnyLong() override { return css::uno::Any(sal_Int32(-123456)); }

    sal_Bool SAL_CALL isAnyLong(css::uno::Any const& value) override
    {
        return value.getValueType() == cppu::UnoType<sal_Int32>::get()
               && *o3tl::forceAccess<sal_Int32>(value) == -123456;
    }

    css::uno::Any SAL_CALL getAnyUnsignedLong() override
    {
        return css::uno::Any(sal_uInt32(3456789012));
    }

    sal_Bool SAL_CALL isAnyUnsignedLong(css::uno::Any const& value) override
    {
        return value.getValueType() == cppu::UnoType<sal_uInt32>::get()
               && *o3tl::forceAccess<sal_uInt32>(value) == 3456789012;
    }

    css::uno::Any SAL_CALL getAnyHyper() override { return css::uno::Any(sal_Int64(-123456789)); }

    sal_Bool SAL_CALL isAnyHyper(css::uno::Any const& value) override
    {
        return value.getValueType() == cppu::UnoType<sal_Int64>::get()
               && *o3tl::forceAccess<sal_Int64>(value) == -123456789;
    }

    css::uno::Any SAL_CALL getAnyUnsignedHyper() override
    {
        return css::uno::Any(sal_uInt64(9876543210));
    }

    sal_Bool SAL_CALL isAnyUnsignedHyper(css::uno::Any const& value) override
    {
        return value.getValueType() == cppu::UnoType<sal_uInt64>::get()
               && *o3tl::forceAccess<sal_uInt64>(value) == 9876543210;
    }

    css::uno::Any SAL_CALL getAnyFloat() override { return css::uno::Any(-10.25f); }

    sal_Bool SAL_CALL isAnyFloat(css::uno::Any const& value) override
    {
        return value.getValueType() == cppu::UnoType<float>::get()
               && *o3tl::forceAccess<float>(value) == -10.25;
    }

    css::uno::Any SAL_CALL getAnyDouble() override { return css::uno::Any(100.5); }

    sal_Bool SAL_CALL isAnyDouble(css::uno::Any const& value) override
    {
        return value.getValueType() == cppu::UnoType<double>::get()
               && *o3tl::forceAccess<double>(value) == 100.5;
    }

    css::uno::Any SAL_CALL getAnyChar() override { return css::uno::Any(u'Ö'); }

    sal_Bool SAL_CALL isAnyChar(css::uno::Any const& value) override
    {
        return value.getValueType() == cppu::UnoType<sal_Unicode>::get()
               && *o3tl::forceAccess<sal_Unicode>(value) == u'Ö';
    }

    css::uno::Any SAL_CALL getAnyString() override { return css::uno::Any(u"hä"_ustr); }

    sal_Bool SAL_CALL isAnyString(css::uno::Any const& value) override
    {
        return value.getValueType() == cppu::UnoType<OUString>::get()
               && *o3tl::forceAccess<OUString>(value) == u"hä";
    }

    css::uno::Any SAL_CALL getAnyType() override
    {
        return css::uno::Any(cppu::UnoType<sal_Int32>::get());
    }

    sal_Bool SAL_CALL isAnyType(css::uno::Any const& value) override
    {
        return value.getValueType() == cppu::UnoType<css::uno::Type>::get()
               && *o3tl::forceAccess<css::uno::Type>(value) == cppu::UnoType<sal_Int32>::get();
    }

    css::uno::Any SAL_CALL getAnySequence() override
    {
        return css::uno::Any(css::uno::Sequence{ u"foo"_ustr, u"barr"_ustr, u"bazzz"_ustr });
    }

    sal_Bool SAL_CALL isAnySequence(css::uno::Any const& value) override
    {
        return value.getValueType() == cppu::UnoType<css::uno::Sequence<OUString>>::get()
               && *o3tl::forceAccess<css::uno::Sequence<OUString>>(value)
                      == css::uno::Sequence<OUString>{ u"foo"_ustr, u"barr"_ustr, u"bazzz"_ustr };
    }

    css::uno::Any SAL_CALL getAnyEnum() override
    {
        return css::uno::Any(org::libreoffice::embindtest::Enum_E_2);
    }

    sal_Bool SAL_CALL isAnyEnum(css::uno::Any const& value) override
    {
        return value.getValueType() == cppu::UnoType<org::libreoffice::embindtest::Enum>::get()
               && *o3tl::forceAccess<org::libreoffice::embindtest::Enum>(value)
                      == org::libreoffice::embindtest::Enum_E_2;
    }

    css::uno::Any SAL_CALL getAnyStruct() override
    {
        return css::uno::Any(org::libreoffice::embindtest::Struct{
            true,
            -12,
            -1234,
            54321,
            -123456,
            3456789012,
            -123456789,
            9876543210,
            -10.25,
            100.5,
            u'Ö',
            u"hä"_ustr,
            cppu::UnoType<sal_Int32>::get(),
            css::uno::Any(sal_Int32(-123456)),
            { u"foo"_ustr, u"barr"_ustr, u"bazzz"_ustr },
            org::libreoffice::embindtest::Enum_E_2,
            { -123456 },
            { { u"foo"_ustr }, -123456, css::uno::Any(sal_Int32(-123456)), { u"barr"_ustr } },
            static_cast<OWeakObject*>(this) });
    }

    sal_Bool SAL_CALL isAnyStruct(css::uno::Any const& value) override
    {
        return value.getValueType() == cppu::UnoType<org::libreoffice::embindtest::Struct>::get()
               && *o3tl::forceAccess<org::libreoffice::embindtest::Struct>(value)
                      == org::libreoffice::embindtest::Struct{
                             true,
                             -12,
                             -1234,
                             54321,
                             -123456,
                             3456789012,
                             -123456789,
                             9876543210,
                             -10.25,
                             100.5,
                             u'Ö',
                             u"hä"_ustr,
                             cppu::UnoType<sal_Int32>::get(),
                             css::uno::Any(sal_Int32(-123456)),
                             { u"foo"_ustr, u"barr"_ustr, u"bazzz"_ustr },
                             org::libreoffice::embindtest::Enum_E_2,
                             { -123456 },
                             { { u"foo"_ustr },
                               -123456,
                               css::uno::Any(sal_Int32(-123456)),
                               { u"barr"_ustr } },
                             static_cast<OWeakObject*>(this)
                         };
    }

    css::uno::Any SAL_CALL getAnyException() override
    {
        return css::uno::Any(org::libreoffice::embindtest::Exception{
            u"error"_ustr, {}, -123456, 100.5, u"hä"_ustr });
    }

    sal_Bool SAL_CALL isAnyException(css::uno::Any const& value) override
    {
        if (value.getValueType() != cppu::UnoType<org::libreoffice::embindtest::Exception>::get())
        {
            return false;
        }
        auto const& e = *o3tl::forceAccess<org::libreoffice::embindtest::Exception>(value);
        return e.Message.startsWith("error") && !e.Context.is() && e.m1 == -123456 && e.m2 == 100.5
               && e.m3 == u"hä";
    }

    css::uno::Any SAL_CALL getAnyInterface() override
    {
        return css::uno::Any(css::uno::Reference<org::libreoffice::embindtest::XTest>(this));
    }

    sal_Bool SAL_CALL isAnyInterface(css::uno::Any const& value) override
    {
        return value.getValueType() == cppu::UnoType<org::libreoffice::embindtest::XTest>::get()
               && *o3tl::forceAccess<css::uno::Reference<org::libreoffice::embindtest::XTest>>(
                      value)
                      == static_cast<OWeakObject*>(this);
    }

    css::uno::Sequence<sal_Bool> SAL_CALL getSequenceBoolean() override
    {
        return { true, true, false };
    }

    sal_Bool SAL_CALL isSequenceBoolean(css::uno::Sequence<sal_Bool> const& value) override
    {
        return value == css::uno::Sequence<sal_Bool>{ true, true, false };
    }

    css::uno::Sequence<sal_Int8> SAL_CALL getSequenceByte() override { return { -12, 1, 12 }; }

    sal_Bool SAL_CALL isSequenceByte(css::uno::Sequence<sal_Int8> const& value) override
    {
        return value == css::uno::Sequence<sal_Int8>{ -12, 1, 12 };
    }

    css::uno::Sequence<sal_Int16> SAL_CALL getSequenceShort() override
    {
        return { -1234, 1, 1234 };
    }

    sal_Bool SAL_CALL isSequenceShort(css::uno::Sequence<sal_Int16> const& value) override
    {
        return value == css::uno::Sequence<sal_Int16>{ -1234, 1, 1234 };
    }

    css::uno::Sequence<sal_uInt16> SAL_CALL getSequenceUnsignedShort() override
    {
        return { 1, 10, 54321 };
    }

    sal_Bool SAL_CALL isSequenceUnsignedShort(css::uno::Sequence<sal_uInt16> const& value) override
    {
        return value == css::uno::Sequence<sal_uInt16>{ 1, 10, 54321 };
    }

    css::uno::Sequence<sal_Int32> SAL_CALL getSequenceLong() override
    {
        return { -123456, 1, 123456 };
    }

    sal_Bool SAL_CALL isSequenceLong(css::uno::Sequence<sal_Int32> const& value) override
    {
        return value == css::uno::Sequence<sal_Int32>{ -123456, 1, 123456 };
    }

    css::uno::Sequence<sal_uInt32> SAL_CALL getSequenceUnsignedLong() override
    {
        return { 1, 10, 3456789012 };
    }

    sal_Bool SAL_CALL isSequenceUnsignedLong(css::uno::Sequence<sal_uInt32> const& value) override
    {
        return value == css::uno::Sequence<sal_uInt32>{ 1, 10, 3456789012 };
    }

    css::uno::Sequence<sal_Int64> SAL_CALL getSequenceHyper() override
    {
        return { -123456789, 1, 123456789 };
    }

    sal_Bool SAL_CALL isSequenceHyper(css::uno::Sequence<sal_Int64> const& value) override
    {
        return value == css::uno::Sequence<sal_Int64>{ -123456789, 1, 123456789 };
    }

    css::uno::Sequence<sal_uInt64> SAL_CALL getSequenceUnsignedHyper() override
    {
        return { 1, 10, 9876543210 };
    }

    sal_Bool SAL_CALL isSequenceUnsignedHyper(css::uno::Sequence<sal_uInt64> const& value) override
    {
        return value == css::uno::Sequence<sal_uInt64>{ 1, 10, 9876543210 };
    }

    css::uno::Sequence<float> SAL_CALL getSequenceFloat() override
    {
        return { -10.25, 1.5, 10.75 };
    }

    sal_Bool SAL_CALL isSequenceFloat(css::uno::Sequence<float> const& value) override
    {
        return value == css::uno::Sequence<float>{ -10.25, 1.5, 10.75 };
    }

    css::uno::Sequence<double> SAL_CALL getSequenceDouble() override
    {
        return { -100.5, 1.25, 100.75 };
    }

    sal_Bool SAL_CALL isSequenceDouble(css::uno::Sequence<double> const& value) override
    {
        return value == css::uno::Sequence<double>{ -100.5, 1.25, 100.75 };
    }

    css::uno::Sequence<sal_Unicode> SAL_CALL getSequenceChar() override
    {
        return { 'a', 'B', u'Ö' };
    }

    sal_Bool SAL_CALL isSequenceChar(css::uno::Sequence<sal_Unicode> const& value) override
    {
        return value == css::uno::Sequence<sal_Unicode>{ 'a', 'B', u'Ö' };
    }

    css::uno::Sequence<OUString> SAL_CALL getSequenceString() override
    {
        return { u"foo"_ustr, u"barr"_ustr, u"bazzz"_ustr };
    }

    sal_Bool SAL_CALL isSequenceString(css::uno::Sequence<OUString> const& value) override
    {
        return value == css::uno::Sequence<OUString>{ u"foo"_ustr, u"barr"_ustr, u"bazzz"_ustr };
    }

    css::uno::Sequence<css::uno::Type> SAL_CALL getSequenceType() override
    {
        return { cppu::UnoType<sal_Int32>::get(), cppu::UnoType<void>::get(),
                 cppu::UnoType<css::uno::Sequence<org::libreoffice::embindtest::Enum>>::get() };
    }

    sal_Bool SAL_CALL isSequenceType(css::uno::Sequence<css::uno::Type> const& value) override
    {
        return value
               == css::uno::Sequence<css::uno::Type>{
                      cppu::UnoType<sal_Int32>::get(), cppu::UnoType<void>::get(),
                      cppu::UnoType<css::uno::Sequence<org::libreoffice::embindtest::Enum>>::get()
                  };
    }

    css::uno::Sequence<css::uno::Any> SAL_CALL getSequenceAny() override
    {
        return { css::uno::Any(sal_Int32(-123456)), css::uno::Any(),
                 css::uno::Any(css::uno::Sequence<org::libreoffice::embindtest::Enum>{
                     org::libreoffice::embindtest::Enum_E_2, org::libreoffice::embindtest::Enum_E3,
                     org::libreoffice::embindtest::Enum_E_10 }) };
    }

    sal_Bool SAL_CALL isSequenceAny(css::uno::Sequence<css::uno::Any> const& value) override
    {
        return value
               == css::uno::Sequence<css::uno::Any>{
                      css::uno::Any(sal_Int32(-123456)), css::uno::Any(),
                      css::uno::Any(css::uno::Sequence<org::libreoffice::embindtest::Enum>{
                          org::libreoffice::embindtest::Enum_E_2,
                          org::libreoffice::embindtest::Enum_E3,
                          org::libreoffice::embindtest::Enum_E_10 })
                  };
    }

    css::uno::Sequence<css::uno::Sequence<OUString>> SAL_CALL getSequenceSequenceString() override
    {
        return { {}, { u"foo"_ustr, u"barr"_ustr }, { u"baz"_ustr } };
    }

    sal_Bool SAL_CALL
    isSequenceSequenceString(css::uno::Sequence<css::uno::Sequence<OUString>> const& value) override
    {
        return value
               == css::uno::Sequence<css::uno::Sequence<OUString>>{ {},
                                                                    { u"foo"_ustr, u"barr"_ustr },
                                                                    { u"baz"_ustr } };
    }

    css::uno::Sequence<org::libreoffice::embindtest::Enum> SAL_CALL getSequenceEnum() override
    {
        return { org::libreoffice::embindtest::Enum_E_2, org::libreoffice::embindtest::Enum_E3,
                 org::libreoffice::embindtest::Enum_E_10 };
    }

    sal_Bool SAL_CALL
    isSequenceEnum(css::uno::Sequence<org::libreoffice::embindtest::Enum> const& value) override
    {
        return value
               == css::uno::Sequence<org::libreoffice::embindtest::Enum>{
                      org::libreoffice::embindtest::Enum_E_2, org::libreoffice::embindtest::Enum_E3,
                      org::libreoffice::embindtest::Enum_E_10
                  };
    }

    css::uno::Sequence<org::libreoffice::embindtest::Struct> SAL_CALL getSequenceStruct() override
    {
        return {
            { true,
              -12,
              -1234,
              1,
              -123456,
              1,
              -123456789,
              1,
              -10.25,
              -100.5,
              'a',
              u"hä"_ustr,
              cppu::UnoType<sal_Int32>::get(),
              css::uno::Any(sal_Int32(-123456)),
              {},
              org::libreoffice::embindtest::Enum_E_2,
              { -123456 },
              { { u"foo"_ustr }, -123456, css::uno::Any(sal_Int32(-123456)), { u"barr"_ustr } },
              static_cast<OWeakObject*>(this) },
            { true,
              1,
              1,
              10,
              1,
              10,
              1,
              10,
              1.5,
              1.25,
              'B',
              u"barr"_ustr,
              cppu::UnoType<void>::get(),
              css::uno::Any(),
              { u"foo"_ustr, u"barr"_ustr },
              org::libreoffice::embindtest::Enum_E3,
              { 1 },
              { { u"baz"_ustr }, 1, css::uno::Any(), { u"foo"_ustr } },
              nullptr },
            { false,
              12,
              1234,
              54321,
              123456,
              3456789012,
              123456789,
              9876543210,
              10.75,
              100.75,
              u'Ö',
              u"bazzz"_ustr,
              cppu::UnoType<css::uno::Sequence<org::libreoffice::embindtest::Enum>>::get(),
              css::uno::Any(css::uno::Sequence<org::libreoffice::embindtest::Enum>{
                  org::libreoffice::embindtest::Enum_E_2, org::libreoffice::embindtest::Enum_E3,
                  org::libreoffice::embindtest::Enum_E_10 }),
              { u"baz"_ustr },
              org::libreoffice::embindtest::Enum_E_10,
              { 123456 },
              { { u"barr"_ustr },
                123456,
                css::uno::Any(css::uno::Sequence<org::libreoffice::embindtest::Enum>{
                    org::libreoffice::embindtest::Enum_E_2, org::libreoffice::embindtest::Enum_E3,
                    org::libreoffice::embindtest::Enum_E_10 }),
                { u"bazz"_ustr } },
              static_cast<OWeakObject*>(this) }
        };
    }

    sal_Bool SAL_CALL
    isSequenceStruct(css::uno::Sequence<org::libreoffice::embindtest::Struct> const& value) override
    {
        return value
               == css::uno::Sequence<org::libreoffice::embindtest::Struct>{
                      { true,
                        -12,
                        -1234,
                        1,
                        -123456,
                        1,
                        -123456789,
                        1,
                        -10.25,
                        -100.5,
                        'a',
                        u"hä"_ustr,
                        cppu::UnoType<sal_Int32>::get(),
                        css::uno::Any(sal_Int32(-123456)),
                        {},
                        org::libreoffice::embindtest::Enum_E_2,
                        { -123456 },
                        { { u"foo"_ustr },
                          -123456,
                          css::uno::Any(sal_Int32(-123456)),
                          { u"barr"_ustr } },
                        static_cast<OWeakObject*>(this) },
                      { true,
                        1,
                        1,
                        10,
                        1,
                        10,
                        1,
                        10,
                        1.5,
                        1.25,
                        'B',
                        u"barr"_ustr,
                        cppu::UnoType<void>::get(),
                        css::uno::Any(),
                        { u"foo"_ustr, u"barr"_ustr },
                        org::libreoffice::embindtest::Enum_E3,
                        { 1 },
                        { { u"baz"_ustr }, 1, css::uno::Any(), { u"foo"_ustr } },
                        nullptr },
                      { false,
                        12,
                        1234,
                        54321,
                        123456,
                        3456789012,
                        123456789,
                        9876543210,
                        10.75,
                        100.75,
                        u'Ö',
                        u"bazzz"_ustr,
                        cppu::UnoType<
                            css::uno::Sequence<org::libreoffice::embindtest::Enum>>::get(),
                        css::uno::Any(css::uno::Sequence<org::libreoffice::embindtest::Enum>{
                            org::libreoffice::embindtest::Enum_E_2,
                            org::libreoffice::embindtest::Enum_E3,
                            org::libreoffice::embindtest::Enum_E_10 }),
                        { u"baz"_ustr },
                        org::libreoffice::embindtest::Enum_E_10,
                        { 123456 },
                        { { u"barr"_ustr },
                          123456,
                          css::uno::Any(css::uno::Sequence<org::libreoffice::embindtest::Enum>{
                              org::libreoffice::embindtest::Enum_E_2,
                              org::libreoffice::embindtest::Enum_E3,
                              org::libreoffice::embindtest::Enum_E_10 }),
                          { u"bazz"_ustr } },
                        static_cast<OWeakObject*>(this) }
                  };
    }

    css::uno::Reference<org::libreoffice::embindtest::XTest> SAL_CALL getNull() override
    {
        return {};
    }

    sal_Bool SAL_CALL
    isNull(css::uno::Reference<org::libreoffice::embindtest::XTest> const& value) override
    {
        return !value;
    }

    void SAL_CALL getOut(sal_Bool& value1, sal_Int8& value2, sal_Int16& value3, sal_uInt16& value4,
                         sal_Int32& value5, sal_uInt32& value6, sal_Int64& value7,
                         sal_uInt64& value8, float& value9, double& value10, sal_Unicode& value11,
                         OUString& value12, css::uno::Type& value13, css::uno::Any& value14,
                         css::uno::Sequence<OUString>& value15,
                         org::libreoffice::embindtest::Enum& value16,
                         org::libreoffice::embindtest::Struct& value17,
                         css::uno::Reference<org::libreoffice::embindtest::XTest>& value18) override
    {
        value1 = true;
        value2 = -12;
        value3 = -1234;
        value4 = 54321;
        value5 = -123456;
        value6 = 3456789012;
        value7 = -123456789;
        value8 = 9876543210;
        value9 = -10.25;
        value10 = 100.5;
        value11 = u'Ö';
        value12 = u"hä"_ustr;
        value13 = cppu::UnoType<sal_Int32>::get();
        value14 <<= sal_Int32(-123456);
        value15 = { u"foo"_ustr, u"barr"_ustr, u"bazzz"_ustr };
        value16 = org::libreoffice::embindtest::Enum_E_2;
        value17
            = { true,
                -12,
                -1234,
                54321,
                -123456,
                3456789012,
                -123456789,
                9876543210,
                -10.25,
                100.5,
                u'Ö',
                u"hä"_ustr,
                cppu::UnoType<sal_Int32>::get(),
                css::uno::Any(sal_Int32(-123456)),
                { u"foo"_ustr, u"barr"_ustr, u"bazzz"_ustr },
                org::libreoffice::embindtest::Enum_E_2,
                { -123456 },
                { { u"foo"_ustr }, -123456, css::uno::Any(sal_Int32(-123456)), { u"barr"_ustr } },
                static_cast<OWeakObject*>(this) };
        value18 = this;
    }

    void SAL_CALL throwRuntimeException() override
    {
        throw css::uno::RuntimeException(u"test"_ustr);
    }

    void SAL_CALL passJob(css::uno::Reference<css::task::XJob> const& object) override
    {
        try
        {
            object->execute({});
        }
        catch (css::uno::RuntimeException& e)
        {
            object->execute({ { u"name"_ustr, css::uno::Any(e.Message) } });
        }
    }

    void SAL_CALL passJobExecutor(css::uno::Reference<css::task::XJobExecutor> const& object,
                                  sal_Bool newThread) override
    {
        if (newThread)
        {
            rtl::Reference<JobExecutorThread> t(new JobExecutorThread(object));
            t->launch();
            t->join();
        }
        else
        {
            object->trigger(u"executor"_ustr);
        }
    }

    void SAL_CALL passInterface(css::uno::Reference<css::uno::XInterface> const& object) override
    {
        css::uno::Reference<css::task::XJob>(object, css::uno::UNO_QUERY_THROW)
            ->execute({ { u"name"_ustr, css::uno::Any(u"queried job"_ustr) } });
        css::uno::Reference<css::task::XJobExecutor>(object, css::uno::UNO_QUERY_THROW)
            ->trigger(u"queried executor"_ustr);
    }

    sal_Bool SAL_CALL checkAttributes(
        css::uno::Reference<org::libreoffice::embindtest::XAttributes> const& object) override
    {
        auto const ok1 = object->getLongAttribute() == 789;
        verify(ok1);
        auto const ok2 = object->getStringAttribute() == u"foo"_ustr;
        verify(ok2);
        bool const ok3 = object->getReadOnlyAttribute();
        verify(ok3);
        return ok1 && ok2 && ok3;
    }

    OUString SAL_CALL getStringAttribute() override { return stringAttribute_; }

    void SAL_CALL setStringAttribute(OUString const& value) override { stringAttribute_ = value; }

    sal_Bool SAL_CALL testSolarMutex() override
    {
        rtl::Reference t(new TestThread);
        t->launch();
        t->join();
        SolarMutexGuard g;
        return t->value;
    }

    OUString stringAttribute_ = u"hä"_ustr;
};

class BridgeTest : public cppu::WeakImplHelper<css::task::XJob>
{
public:
    explicit BridgeTest(css::uno::Reference<css::uno::XComponentContext> const& context)
        : context_(context)
    {
    }

private:
    css::uno::Any SAL_CALL
    execute(css::uno::Sequence<css::beans::NamedValue> const& Arguments) override
    {
        if (Arguments.hasElements())
        {
            throw css::lang::IllegalArgumentException(u"BridgeTest execute args not empty"_ustr, {},
                                                      0);
        }
        auto const envCppOrig = css::uno::Environment::getCurrent();
        css::uno::Environment envUno;
        uno_createEnvironment(reinterpret_cast<uno_Environment**>(&envUno),
                              u"" UNO_LB_UNO ""_ustr.pData, nullptr);
        css::uno::Mapping cpp2uno(envCppOrig.get(), envUno.get());
        css::uno::Environment envCpp;
        if (!cpp2uno.is())
        {
            throw css::uno::RuntimeException(u"cannot get C++ to UNO mapping"_ustr);
        }
        uno_createEnvironment(reinterpret_cast<uno_Environment**>(&envCpp),
                              envCppOrig.getTypeName().pData, nullptr);
        css::uno::Mapping uno2cpp(envUno.get(), envCpp.get());
        if (!uno2cpp.is())
        {
            throw css::uno::RuntimeException(u"cannot get UNO to C++ mapping"_ustr);
        }
        css::uno::UnoInterfaceReference ifcUno;
        cpp2uno.mapInterface(reinterpret_cast<void**>(&ifcUno.m_pUnoI),
                             org::libreoffice::embindtest::Test::create(context_).get(),
                             cppu::UnoType<org::libreoffice::embindtest::XTest>::get());
        if (!ifcUno.is())
        {
            throw css::uno::RuntimeException(u"cannot map from C++ to UNO"_ustr);
        }
        css::uno::Reference<org::libreoffice::embindtest::XTest> ifcCpp;
        uno2cpp.mapInterface(reinterpret_cast<void**>(&ifcCpp), ifcUno.get(),
                             cppu::UnoType<org::libreoffice::embindtest::XTest>::get());
        if (!ifcCpp.is())
        {
            throw css::uno::RuntimeException(u"cannot map from UNO to C++"_ustr);
        }
        {
            bool const val = ifcCpp->getBoolean();
            verify(val);
            bool const ok = ifcCpp->isBoolean(val);
            verify(ok);
        }
        {
            auto const val = ifcCpp->getByte();
            verify(val == -12);
            bool const ok = ifcCpp->isByte(val);
            verify(ok);
        }
        {
            auto const val = ifcCpp->getShort();
            verify(val == -1234);
            bool const ok = ifcCpp->isShort(val);
            verify(ok);
        }
        {
            auto const val = ifcCpp->getUnsignedShort();
            verify(val == 54321);
            bool const ok = ifcCpp->isUnsignedShort(val);
            verify(ok);
        }
        {
            auto const val = ifcCpp->getLong();
            verify(val == -123456);
            bool const ok = ifcCpp->isLong(val);
            verify(ok);
        }
        {
            auto const val = ifcCpp->getUnsignedLong();
            verify(val == 3456789012);
            bool const ok = ifcCpp->isUnsignedLong(val);
            verify(ok);
        }
        {
            auto const val = ifcCpp->getHyper();
            verify(val == -123456789);
            bool const ok = ifcCpp->isHyper(val);
            verify(ok);
        }
        {
            auto const val = ifcCpp->getUnsignedHyper();
            verify(val == 9876543210);
            bool const ok = ifcCpp->isUnsignedHyper(val);
            verify(ok);
        }
        {
            auto const val = ifcCpp->getFloat();
            verify(val == -10.25);
            bool const ok = ifcCpp->isFloat(val);
            verify(ok);
        }
        {
            auto const val = ifcCpp->getDouble();
            verify(val == 100.5);
            bool const ok = ifcCpp->isDouble(val);
            verify(ok);
        }
        {
            auto const val = ifcCpp->getChar();
            verify(val == u'Ö');
            bool const ok = ifcCpp->isChar(val);
            verify(ok);
        }
        {
            auto const val = ifcCpp->getString();
            verify(val == u"hä"_ustr);
            bool const ok = ifcCpp->isString(val);
            verify(ok);
        }
        {
            auto const val = ifcCpp->getType();
            verify(val == cppu::UnoType<sal_Int32>::get());
            bool const ok = ifcCpp->isType(val);
            verify(ok);
        }
        {
            auto const val = ifcCpp->getEnum();
            verify(val == org::libreoffice::embindtest::Enum_E_2);
            bool const ok = ifcCpp->isEnum(val);
            verify(ok);
        }
        {
            auto const val = ifcCpp->getStruct();
            verify(val
                   == org::libreoffice::embindtest::Struct{
                          true,
                          -12,
                          -1234,
                          54321,
                          -123456,
                          3456789012,
                          -123456789,
                          9876543210,
                          -10.25,
                          100.5,
                          u'Ö',
                          u"hä"_ustr,
                          cppu::UnoType<sal_Int32>::get(),
                          css::uno::Any(sal_Int32(-123456)),
                          { u"foo"_ustr, u"barr"_ustr, u"bazzz"_ustr },
                          org::libreoffice::embindtest::Enum_E_2,
                          { -123456 },
                          { { u"foo"_ustr },
                            -123456,
                            css::uno::Any(sal_Int32(-123456)),
                            { u"barr"_ustr } },
                          ifcCpp });
            bool const ok = ifcCpp->isStruct(val);
            verify(ok);
        }
        {
            auto const val = ifcCpp->getStructLong();
            verify(val == org::libreoffice::embindtest::StructLong{ -123456 });
            bool const ok = ifcCpp->isStructLong(val);
            verify(ok);
        }
        {
            auto const val = ifcCpp->getStructString();
            verify(val == org::libreoffice::embindtest::StructString{ u"hä"_ustr });
            bool const ok = ifcCpp->isStructString(val);
            verify(ok);
        }
        {
            auto const val = ifcCpp->getAnyVoid();
            verify(val == css::uno::Any());
            bool const ok = ifcCpp->isAnyVoid(val);
            verify(ok);
        }
        {
            auto const val = ifcCpp->getSequenceBoolean();
            verify(val == css::uno::Sequence<sal_Bool>{ true, true, false });
            bool const ok = ifcCpp->isSequenceBoolean(val);
            verify(ok);
        }
        {
            auto const val = ifcCpp->getNull();
            verify(val == css::uno::Reference<org::libreoffice::embindtest::XTest>());
            bool const ok = ifcCpp->isNull(val);
            verify(ok);
        }
        {
            sal_Bool value1;
            sal_Int8 value2;
            sal_Int16 value3;
            sal_uInt16 value4;
            sal_Int32 value5;
            sal_uInt32 value6;
            sal_Int64 value7;
            sal_uInt64 value8;
            float value9;
            double value10;
            sal_Unicode value11;
            OUString value12;
            css::uno::Type value13;
            css::uno::Any value14;
            css::uno::Sequence<OUString> value15;
            org::libreoffice::embindtest::Enum value16;
            org::libreoffice::embindtest::Struct value17;
            css::uno::Reference<org::libreoffice::embindtest::XTest> value18;
            ifcCpp->getOut(value1, value2, value3, value4, value5, value6, value7, value8, value9,
                           value10, value11, value12, value13, value14, value15, value16, value17,
                           value18);
            verify(value1);
            verify(value2 == -12);
            verify(value3 == -1234);
            verify(value4 == 54321);
            verify(value5 == -123456);
            verify(value6 == 3456789012);
            verify(value7 == -123456789);
            verify(value8 == 9876543210);
            verify(value9 == -10.25);
            verify(value10 == 100.5);
            verify(value11 == u'Ö');
            verify(value12 == u"hä"_ustr);
            verify(value13 == cppu::UnoType<sal_Int32>::get());
            verify(value14 == css::uno::Any(sal_Int32(-123456)));
            verify(value15
                   == css::uno::Sequence<OUString>{ u"foo"_ustr, u"barr"_ustr, u"bazzz"_ustr });
            verify(value16 == org::libreoffice::embindtest::Enum_E_2);
            verify(value17
                   == org::libreoffice::embindtest::Struct{
                          true,
                          -12,
                          -1234,
                          54321,
                          -123456,
                          3456789012,
                          -123456789,
                          9876543210,
                          -10.25,
                          100.5,
                          u'Ö',
                          u"hä"_ustr,
                          cppu::UnoType<sal_Int32>::get(),
                          css::uno::Any(sal_Int32(-123456)),
                          { u"foo"_ustr, u"barr"_ustr, u"bazzz"_ustr },
                          org::libreoffice::embindtest::Enum_E_2,
                          { -123456 },
                          { { u"foo"_ustr },
                            -123456,
                            css::uno::Any(sal_Int32(-123456)),
                            { u"barr"_ustr } },
                          ifcCpp });
            verify(value18 == ifcCpp);
        }
        try
        {
            ifcCpp->throwRuntimeException();
            verify(false);
        }
        catch (css::uno::RuntimeException& e)
        {
            verify(e.Message.startsWith("test"));
        }
        {
            auto const val1 = ifcCpp->getStringAttribute();
            verify(val1 == u"hä"_ustr);
            ifcCpp->setStringAttribute(u"foo"_ustr);
            auto const val2 = ifcCpp->getStringAttribute();
            verify(val2 == u"foo"_ustr);
        }
        return css::uno::Any(true);
    }

    css::uno::Reference<css::uno::XComponentContext> context_;
};
}

extern "C" SAL_DLLPUBLIC_EXPORT css::uno::XInterface*
org_libreoffice_comp_embindtest_BridgeTest_get_implementation(
    css::uno::XComponentContext* context, css::uno::Sequence<css::uno::Any> const&)
{
    return cppu::acquire(new BridgeTest(context));
}

extern "C" SAL_DLLPUBLIC_EXPORT css::uno::XInterface*
org_libreoffice_comp_embindtest_Test_get_implementation(css::uno::XComponentContext*,
                                                        css::uno::Sequence<css::uno::Any> const&)
{
    return cppu::acquire(new Test);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
