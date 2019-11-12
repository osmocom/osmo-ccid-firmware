#!/usr/bin/python3

# This script checks your libccid configuration file if it contains a matching entry
# for the sysmoOCTSIM reader.  If not, it will generate a modified config file

import plistlib, sys

INFILE="/etc/libccid_Info.plist"
OUTFILE="/tmp/libccid_Info.plist"

VENDOR_ID=0x1d50
PRODUCT_ID=0x6141
NAME='sysmocom sysmoOCTSIM'

def gen_reader_dictlist(prod_id, vend_id, names):
    readers = []
    for i in range(0,len(prod_id)):
        reader = {'vendor_id': vend_id[i], 'product_id': prod_id[i], 'name': names[i]}
        readers.append(reader)
    return readers

def find_reader(readers, vend_id, prod_id):
    for r in readers:
        if int(r['vendor_id'], 16) == vend_id and int(r['product_id'], 16) == prod_id:
            return r
    return None

def plist_add_reader(pl, vend_id, prod_id, name):
    pl['ifdVendorID'].append(hex(vend_id))
    pl['ifdProductID'].append(hex(prod_id))
    pl['ifdFriendlyName'].append(name)


if len(sys.argv) > 1:
    INFILE = sys.argv[1]
if len(sys.argv) > 2:
    OUTFILE = sys.argv[2]

# read the property list
print("Reading libccid config file at '%s'" % (INFILE))
with open(INFILE, 'rb') as fp:
    pl = plistlib.load(fp)

# consistency check
if len(pl['ifdProductID']) != len(pl['ifdVendorID']) or len(pl['ifdProductID']) != len(pl['ifdFriendlyName']):
    print("input file is corrupt", file=sys.stderr)
    sys.exit(2)

# convert into a better sorted form (one list of dicts; each dict one reader)
readers = gen_reader_dictlist(pl['ifdProductID'], pl['ifdVendorID'], pl['ifdFriendlyName'])

if find_reader(readers, VENDOR_ID, PRODUCT_ID):
    print("Matching reader already in libccid_Info.plist; no action required", file=sys.stderr)
else:
    print("Reader not found in config file, it needs to be updated...")
    plist_add_reader(pl, VENDOR_ID, PRODUCT_ID, NAME)
    with open(OUTFILE, 'wb') as fp:
        plistlib.dump(pl, fp)
    print("Generated new config file stored as '%s'" % (OUTFILE))
    print("\tWARNING: The generated file doesn't preserve comments!")
