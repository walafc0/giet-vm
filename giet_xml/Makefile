
all: xml2bin bin2xml

bin2xml: xml_driver.c mapping_info.h
	gcc -Wall -I. -I.. xml_driver.c -o bin2xml

xml2bin: xml_parser.c mapping_info.h 
	gcc -Wall -I. -I.. -I../giet_kernel -I/usr/include/libxml2 xml_parser.c -o xml2bin -lxml2

test:
	./xml2bin map.xml test.bin
	./bin2xml test.bin test.xml

up_test:
	./xml2bin ../map.xml ../test.bin
	./bin2xml ../test.bin ../test.xml


clean:
	rm -f xml2bin bin2xml
