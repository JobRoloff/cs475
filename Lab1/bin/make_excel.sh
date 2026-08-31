#!/bin/bash
#
# make_excel.sh - build sweep_results.xlsx from a sweep CSV, adding
#                 "median" and "speedup" columns. Pure bash + awk + zip,
#                 no Python involved.
#
# Usage:
#   ./make_excel.sh [input.csv] [output.xlsx]
#     input.csv   default: sweep_results.csv   (from sweep.csh)
#     output.xlsx default: sweep_results.xlsx
#
# CSV format expected (from sweep.csh): one row per data point, first
# column is a label ("SEQ" or a thread count p), followed by trial times.
# The first row (SEQ) and the row labeled "1" (single OMP thread) are
# pooled together to find the fastest single-core-equivalent baseline --
# OMP with 1 thread is sometimes faster than plain SEQ, so we don't assume
# SEQ is automatically the right baseline.

set -e

INFILE="${1:-sweep_results.csv}"
OUTFILE="${2:-sweep_results.xlsx}"

if [ ! -f "$INFILE" ]; then
    echo "Error: input CSV '$INFILE' not found."
    exit 1
fi

WORKDIR=$(mktemp -d)
trap 'rm -rf "$WORKDIR"' EXIT

#############################################################################
# 1. Compute median (per row) and speedup (baseline / median) with awk.
#    Baseline = min over all trial values in the SEQ row and the p=1 row.
#    Output: same rows as input, with "median" and "speedup" columns added.
#############################################################################

PROCESSED="$WORKDIR/processed.csv"

awk -F',' -v OFS=',' '
NR == 1 {
    header = $0
    ntrials = NF - 1
    print header, "median", "speedup"
    next
}
{
    label = $1
    n = NF - 1
    for (i = 1; i <= n; i++) {
        vals[i] = $(i + 1) + 0
    }
    # insertion sort (n is small -- number of trials)
    for (i = 2; i <= n; i++) {
        key = vals[i]
        j = i - 1
        while (j >= 1 && vals[j] > key) {
            vals[j + 1] = vals[j]
            j--
        }
        vals[j + 1] = key
    }
    if (n % 2 == 1) {
        med = vals[(n + 1) / 2]
    } else {
        med = (vals[n / 2] + vals[n / 2 + 1]) / 2.0
    }

    rows = rows + 1
    rowline[rows] = $0
    rowmedian[rows] = med

    if (label == "SEQ" || label == "1") {
        for (i = 1; i <= n; i++) {
            v = $(i + 1) + 0
            if (baseline_set == 0 || v < baseline) {
                baseline = v
                baseline_set = 1
            }
        }
    }
}
END {
    for (r = 1; r <= rows; r++) {
        speedup = baseline / rowmedian[r]
        printf "%s,%.6f,%.4f\n", rowline[r], rowmedian[r], speedup
    }
}
' "$INFILE" > "$PROCESSED"

#############################################################################
# 2. Build the OOXML parts for a minimal, valid .xlsx
#############################################################################

mkdir -p "$WORKDIR/xl/worksheets" "$WORKDIR/xl/_rels" "$WORKDIR/_rels"

cat > "$WORKDIR/[Content_Types].xml" << 'EOF'
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>
<Default Extension="xml" ContentType="application/xml"/>
<Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/>
<Override PartName="/xl/worksheets/sheet1.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/>
<Override PartName="/xl/styles.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml"/>
</Types>
EOF

cat > "$WORKDIR/_rels/.rels" << 'EOF'
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/>
</Relationships>
EOF

cat > "$WORKDIR/xl/_rels/workbook.xml.rels" << 'EOF'
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/>
<Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles" Target="styles.xml"/>
</Relationships>
EOF

cat > "$WORKDIR/xl/workbook.xml" << 'EOF'
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">
<sheets>
<sheet name="sweep_results" sheetId="1" r:id="rId1"/>
</sheets>
</workbook>
EOF

cat > "$WORKDIR/xl/styles.xml" << 'EOF'
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<styleSheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">
<fonts count="1"><font><sz val="11"/><name val="Calibri"/></font></fonts>
<fills count="1"><fill><patternFill patternType="none"/></fill></fills>
<borders count="1"><border><left/><right/><top/><bottom/><diagonal/></border></borders>
<cellStyleXfs count="1"><xf numFmtId="0" fontId="0" fillId="0" borderId="0"/></cellStyleXfs>
<cellXfs count="1"><xf numFmtId="0" fontId="0" fillId="0" borderId="0" xfId="0"/></cellXfs>
<cellStyles count="1"><cellStyle name="Normal" xfId="0" builtinId="0"/></cellStyles>
</styleSheet>
EOF

# Convert a 1-based column index to spreadsheet column letters (1->A, 27->AA, ...)
colletter() {
    local n=$1
    local letters=""
    while [ "$n" -gt 0 ]; do
        local rem=$(( (n - 1) % 26 ))
        letters="$(printf \\$(printf '%03o' $((65 + rem))))$letters"
        n=$(( (n - 1) / 26 ))
    done
    echo "$letters"
}

SHEET="$WORKDIR/xl/worksheets/sheet1.xml"
{
    echo '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
    echo '<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><sheetData>'
} > "$SHEET"

rownum=0
while IFS= read -r line; do
    rownum=$((rownum + 1))
    IFS=',' read -ra fields <<< "$line"
    printf '<row r="%d">' "$rownum" >> "$SHEET"
    colnum=0
    for field in "${fields[@]}"; do
        colnum=$((colnum + 1))
        col=$(colletter "$colnum")
        ref="${col}${rownum}"
        # numeric if it parses as a number, otherwise inline string
        if [[ "$field" =~ ^-?[0-9]+([.][0-9]+)?$ ]]; then
            printf '<c r="%s"><v>%s</v></c>' "$ref" "$field" >> "$SHEET"
        else
            printf '<c r="%s" t="inlineStr"><is><t>%s</t></is></c>' "$ref" "$field" >> "$SHEET"
        fi
    done
    printf '</row>' >> "$SHEET"
done < "$PROCESSED"

echo '</sheetData></worksheet>' >> "$SHEET"

#############################################################################
# 3. Zip it all up into the .xlsx
#############################################################################

rm -f "$OUTFILE"
( cd "$WORKDIR" && zip -q -X -r "$OUTFILE" "[Content_Types].xml" _rels xl )
mv "$WORKDIR/$OUTFILE" "$OUTFILE"

echo "Wrote $OUTFILE"
