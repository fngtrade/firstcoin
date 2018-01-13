#!/bin/sh

TOPDIR=${TOPDIR:-$(git rev-parse --show-toplevel)}
SRCDIR=${SRCDIR:-$TOPDIR/src}
MANDIR=${MANDIR:-$TOPDIR/doc/man}

FIRSTCOIND=${FIRSTCOIND:-$SRCDIR/firstcoind}
FIRSTCOINCLI=${FIRSTCOINCLI:-$SRCDIR/firstcoin-cli}
FIRSTCOINTX=${FIRSTCOINTX:-$SRCDIR/firstcoin-tx}
FIRSTCOINQT=${FIRSTCOINQT:-$SRCDIR/qt/firstcoin-qt}

[ ! -x $FIRSTCOIND ] && echo "$FIRSTCOIND not found or not executable." && exit 1

# The autodetected version git tag can screw up manpage output a little bit
BTCVER=($($FIRSTCOINCLI --version | head -n1 | awk -F'[ -]' '{ print $6, $7 }'))

# Create a footer file with copyright content.
# This gets autodetected fine for firstcoind if --version-string is not set,
# but has different outcomes for firstcoin-qt and firstcoin-cli.
echo "[COPYRIGHT]" > footer.h2m
$FIRSTCOIND --version | sed -n '1!p' >> footer.h2m

for cmd in $FIRSTCOIND $FIRSTCOINCLI $FIRSTCOINTX $FIRSTCOINQT; do
  cmdname="${cmd##*/}"
  help2man -N --version-string=${BTCVER[0]} --include=footer.h2m -o ${MANDIR}/${cmdname}.1 ${cmd}
  sed -i "s/\\\-${BTCVER[1]}//g" ${MANDIR}/${cmdname}.1
done

rm -f footer.h2m
