CXX         = g++
DAISYSP_DIR = ../../lib/Aurora-SDK/libs/DaisySP
CXXFLAGS    = -std=c++14 -Wall -I.. -I$(DAISYSP_DIR)/Source -I$(DAISYSP_DIR)/Source/Utility

DOCTEST_URL = https://raw.githubusercontent.com/doctest/doctest/master/doctest/doctest.h
DOCTEST_H   = doctest.h

DAISYSP_SOURCES = $(DAISYSP_DIR)/Source/Effects/chorus.cpp \
                   $(DAISYSP_DIR)/Source/Effects/flanger.cpp \
                   $(DAISYSP_DIR)/Source/Effects/phaser.cpp

all: $(DOCTEST_H) test_modulation test_blend_colour test_led_breath
	./test_modulation
	./test_blend_colour
	./test_led_breath

$(DOCTEST_H):
	curl -sSL $(DOCTEST_URL) -o $@

test_modulation: test_modulation.cpp ../modulation.cpp ../modulation.h $(DOCTEST_H)
	$(CXX) $(CXXFLAGS) -o $@ test_modulation.cpp ../modulation.cpp $(DAISYSP_SOURCES)

test_blend_colour: test_blend_colour.cpp ../blend_colour.h $(DOCTEST_H)
	$(CXX) $(CXXFLAGS) -o $@ $<

test_led_breath: test_led_breath.cpp ../led_breath.h ../modulation.h $(DOCTEST_H)
	$(CXX) $(CXXFLAGS) -o $@ test_led_breath.cpp

clean:
	rm -f test_modulation test_blend_colour test_led_breath $(DOCTEST_H)
