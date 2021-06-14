/*
 * Copyright (c) 2020-2021 The reone project contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/** @file
 *  Tests for libcommon classes.
 */

#define BOOST_TEST_MODULE common

#include <boost/test/included/unit_test.hpp>

#include "../engine/common/streamreader.h"
#include "../engine/common/timer.h"

using namespace std;

using namespace reone;

namespace endian = boost::endian;

// StreamReader

BOOST_AUTO_TEST_CASE(StreamReader_GetLE) {
    auto stream = make_shared<istringstream>(string("\x01" "\xe8\x03" "\xa0\x86\x01\x00" "\x00\xe4\x0b\x54\x02\x00\x00\x00" "\x60\x79\xfe\xff" "\x00\x00\x80\x3f" "abc\0defgh", 32));
    StreamReader reader(stream);
    BOOST_TEST((reader.getByte() == 0x01));
    BOOST_TEST((reader.getUint16() == 1000u));
    BOOST_TEST((reader.getUint32() == 100000u));
    BOOST_TEST((reader.getUint64() == 10000000000u));
    BOOST_TEST((reader.getInt32() == -100000));
    BOOST_TEST((reader.getFloat() == 1.0f));
    BOOST_TEST((reader.getCString() == "abc"));
    BOOST_TEST((reader.getString(3) == "def"));
}

BOOST_AUTO_TEST_CASE(StreamReader_GetBE) {
    auto stream = make_shared<istringstream>(string("\x03\xe8" "\x00\x01\x86\xa0" "\x00\x00\x00\x02\x54\x0b\xe4\x00" "\xff\xfe\x79\x60" "\x3f\x80\x00\x00", 22));
    StreamReader reader(stream, endian::order::big);
    BOOST_TEST((reader.getUint16() == 1000u));
    BOOST_TEST((reader.getUint32() == 100000u));
    BOOST_TEST((reader.getUint64() == 10000000000u));
    BOOST_TEST((reader.getInt32() == -100000));
    BOOST_TEST((reader.getFloat() == 1.0f));
}

// END StreamReader

// Timer

BOOST_AUTO_TEST_CASE(Timer_TimesOut) {
    Timer timer(1.0f);

    timer.advance(0.5f);

    BOOST_TEST(!timer.isTimedOut());

    timer.advance(0.6f);

    BOOST_TEST(timer.isTimedOut());
}

// END Timer
