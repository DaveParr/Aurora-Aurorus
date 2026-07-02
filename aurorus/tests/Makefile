CXX      = g++
CXXFLAGS = -std=c++14 -Wall -I..

DOCTEST_URL = https://raw.githubusercontent.com/doctest/doctest/master/doctest/doctest.h
DOCTEST_H   = doctest.h

all: $(DOCTEST_H) test_modulation test_blend_colour
	./test_modulation
	./test_blend_colour

$(DOCTEST_H):
	curl -sSL $(DOCTEST_URL) -o $@

test_modulation: test_modulation.cpp ../modulation.h $(DOCTEST_H)
	$(CXX) $(CXXFLAGS) -o $@ test_modulation.cpp

test_blend_colour: test_blend_colour.cpp ../blend_colour.h $(DOCTEST_H)
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f test_modulation test_blend_colour $(DOCTEST_H)
