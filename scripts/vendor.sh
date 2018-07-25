#!/sbin/sh

# /sbin/sh runs out of TWRP.. use it.

PROGS="sgdisk toybox resize2fs e2fsck mke2fs"
RC=0
# Check for my needed programs
for PROG in ${PROGS} ; do
   if [ ! -x "/tmp/${PROG}" ] ; then
      echo "Missing: /tmp/${PROG}"
      RC=9
   fi
done
#
# all prebuilt
if [ ${RC} -ne 0 ] ; then
   echo "Aborting.."
   exit 7
fi

TOYBOX="/tmp/toybox"

# Get bootdevice.. don't assume /dev/block/sda
DISK=`${TOYBOX} readlink /dev/block/bootdevice/by-name/system | ${TOYBOX} sed -e's/[0-9]//g'`

# Check for /vendor existence
VENDOR=`/tmp/sgdisk --pretend --print ${DISK} | ${TOYBOX} grep -c vendor`

if [ ${VENDOR} -ge 1 ] ; then
# Got it, we're done...
   exit 0
fi

# Missing... need to create it..
${TOYBOX} echo "/vendor missing"
#
# Get next partition...
LAST=`/tmp/sgdisk --pretend --print ${DISK} | ${TOYBOX} tail -1 | ${TOYBOX} tr -s ' ' | ${TOYBOX} cut -d' ' -f2`
NEXT=`${TOYBOX} expr ${LAST} + 1`
NUMPARTS=`/tmp/sgdisk --pretend --print ${DISK} | ${TOYBOX} grep 'holds up to' | ${TOYBOX} tr -s ' ' | ${TOYBOX} cut -d' ' -f6`

# Check if we need to expand the partition table
RESIZETABLE=""
if [ ${NEXT} -gt ${NUMPARTS} ] ; then
   RESIZETABLE=" --resize-table=${NEXT}"
fi

# Get /system partition #, start, ending, code
SYSPARTNUM=`/tmp/sgdisk --pretend --print ${DISK} | ${TOYBOX} grep system | ${TOYBOX} tr -s ' ' | ${TOYBOX} cut -d' ' -f2`
SYSSTART=`/tmp/sgdisk --pretend --print ${DISK} | ${TOYBOX} grep system | ${TOYBOX} tr -s ' ' | ${TOYBOX} cut -d' ' -f3`
SYSEND=`/tmp/sgdisk --pretend --print ${DISK} | ${TOYBOX} grep system | ${TOYBOX} tr -s ' ' | ${TOYBOX} cut -d' ' -f4`
SYSCODE=`/tmp/sgdisk --pretend --print ${DISK} | ${TOYBOX} grep system | ${TOYBOX} tr -s ' ' | ${TOYBOX} cut -d' ' -f7`

# Get sector size
SECSIZE=`/tmp/sgdisk --pretend --print ${DISK} | ${TOYBOX} grep 'sector size' | ${TOYBOX} tr -s ' ' | ${TOYBOX} cut -d' ' -f4`

## Resize part..
/tmp/e2fsck /dev/block/bootdevice/by-name/system

# 512 = 512mb..
VENDORSIZE=`${TOYBOX} expr 512 \* 1024 \* 1024 / ${SECSIZE}`

NEWEND=`${TOYBOX} expr ${SYSEND} - ${VENDORSIZE}`
VENDORSTART=`${TOYBOX} expr ${NEWEND} + 1`

NEWSYSSIZE=`${TOYBOX} expr ${NEWEND} - ${SYSSTART} + 1`
MINSYSSIZE=`/tmp/resize2fs -P /dev/block/bootdevice/by-name/system 2>/dev/null | ${TOYBOX} grep minimum | ${TOYBOX} tr -s ' ' | ${TOYBOX} cut -d' ' -f7`

# Check if /system will shrink to small
if [ ${NEWSYSSIZE} -lt 0 ] ; then
   echo "ERROR: /system will be smaller than 0."
   exit 9
fi
if [ ${NEWSYSSIZE} -lt ${MINSYSSIZE} ] ; then
   echo "ERROR: /system will be smaller than the minimum allowed."
   exit 9
fi

# Resize /system, this will preserve the data and shrink it.
${TOYBOX} echo "*********Resize /system to ${NEWSYSSIZE} = ${NEWEND} - ${SYSSTART} + 1 (inclusize) = ${NEWSYSSIZE}"

### TO REALLY DO THIS, REMOVE THE echo ###
/tmp/e2fsck -y -f /dev/block/bootdevice/by-name/system
### TO REALLY DO THIS, REMOVE THE echo ###
/tmp/resize2fs /dev/block/bootdevice/by-name/system ${NEWSYSSIZE}

# Only echo's for now... --pretend will NOT do it..
### TO REALLY DO THIS, REMOVE THE --pretend ###
/tmp/sgdisk ${RESIZETABLE} --delete=${SYSPARTNUM} --new=${SYSPARTNUM}:${SYSSTART}:${NEWEND} --change-name=${SYSPARTNUM}:system --new=${NEXT}:${VENDORSTART}:${SYSEND} --change-name=${NEXT}:vendor --print ${DISK}

echo /tmp/sgdisk --pretend ${RESIZETABLE} --delete=${SYSPARTNUM} --new=${SYSPARTNUM}:${SYSSTART}:${NEWEND} --change-name=${SYSPARTNUM}:system --new=${NEXT}:${VENDORSTART}:${SYSEND} --change-name=${NEXT}:vendor --print ${DISK}

## REALLY DO THIS ##
/tmp/mke2fs -t ext4 ${DISK}${NEXT}
