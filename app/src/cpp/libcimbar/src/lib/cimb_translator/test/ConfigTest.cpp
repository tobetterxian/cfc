/* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
#include "unittest.h"

#include "Config.h"

TEST_CASE( "ConfigTest/testHighDensityModes", "[unit]" )
{
	const cimbar::conf conf8x8 = cimbar::Config::temp_conf(68);
	const cimbar::conf conf5x5 = cimbar::Config::temp_conf(69);
	const cimbar::conf conf5x5d = cimbar::Config::temp_conf(70);

	assertEquals(5, (int)conf5x5.cell_size);
	assertEquals(988, (int)conf5x5.image_size_x);
	assertEquals(216, (int)conf5x5.ecc_block_size);
	assertEquals(5, (int)conf5x5d.cell_size);
	assertEquals(958, (int)conf5x5d.image_size_x);
	assertEquals(182, (int)conf5x5d.ecc_block_size);

	assertTrue(conf5x5.capacity() > conf8x8.capacity());
	assertTrue(conf5x5d.capacity() > conf8x8.capacity());
}
