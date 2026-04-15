#!/bin/bash
# download_gutenberg.sh
# Descarga los Top 100 libros (ultimos 30 dias) del Proyecto Gutenberg
# en formato texto plano UTF-8.
#
# Uso:
#   bash download_gutenberg.sh
#
# Resultado:
#   gutenberg_books/book_NNNN.txt  (uno por libro descargado)

set -e

OUTPUT_DIR="gutenberg_books"
IDS_FILE="/tmp/gutenberg_ids.txt"
TOP_URL="https://www.gutenberg.org/browse/scores/top"
MAX_BOOKS=100
DELAY=1   # segundos entre descargas (respetar el servidor)

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

mkdir -p "$OUTPUT_DIR"

# ─────────────────────────────────────────────────────────────────────────────
# PASO 1: Descargar la pagina de top 100 y extraer los IDs del apartado
#         "books-last30"
# ─────────────────────────────────────────────────────────────────────────────
echo -e "${YELLOW}[1/3] Descargando lista de top 100 libros...${NC}"
echo "      URL: $TOP_URL"

# Descargamos la pagina HTML completa
HTML=$(curl -s --max-time 30 -A "Mozilla/5.0" "$TOP_URL")

if [ -z "$HTML" ]; then
    echo -e "${RED}ERROR: No se pudo descargar la pagina del top 100.${NC}"
    exit 1
fi

# La pagina tiene secciones con id="books-last30".
# Extraemos el bloque HTML que va desde "books-last30" hasta la siguiente
# seccion principal, y de ese bloque tomamos los /ebooks/NUMERO
echo "$HTML" | \
    awk '/id="books-last30"/{found=1} found && /id="books-last1"/{found=0} found{print}' | \
    grep -oE '/ebooks/[0-9]+' | \
    grep -oE '[0-9]+' | \
    sort -u | \
    head -n "$MAX_BOOKS" > "$IDS_FILE"

TOTAL=$(wc -l < "$IDS_FILE" | tr -d ' ')
echo -e "${GREEN}      Encontrados $TOTAL IDs de libros.${NC}"

if [ "$TOTAL" -eq 0 ]; then
    # Fallback: tomar todos los /ebooks/ de la pagina completa
    echo -e "${YELLOW}      Fallback: extrayendo IDs de la pagina completa...${NC}"
    echo "$HTML" | \
        grep -oE '/ebooks/[0-9]+' | \
        grep -oE '[0-9]+' | \
        sort -u | \
        head -n "$MAX_BOOKS" > "$IDS_FILE"
    TOTAL=$(wc -l < "$IDS_FILE" | tr -d ' ')
    echo -e "${GREEN}      Encontrados $TOTAL IDs.${NC}"
fi

echo ""

# ─────────────────────────────────────────────────────────────────────────────
# PASO 2: Descargar cada libro como texto plano UTF-8
# ─────────────────────────────────────────────────────────────────────────────
echo -e "${YELLOW}[2/3] Descargando libros...${NC}"

OK=0
FAIL=0
COUNT=0

while read -r book_id; do
    COUNT=$((COUNT + 1))
    OUT="$OUTPUT_DIR/book_${book_id}.txt"

    if [ -f "$OUT" ] && [ -s "$OUT" ]; then
        echo -e "  [${COUNT}/${TOTAL}] ${GREEN}Ya existe:${NC} book_${book_id}.txt"
        OK=$((OK + 1))
        continue
    fi

    printf "  [%d/%d] Libro %-6s ... " "$COUNT" "$TOTAL" "$book_id"

    # Intentar URL primaria (formato -0.txt = UTF-8 sin BOM, el mas comun)
    URL1="https://www.gutenberg.org/files/${book_id}/${book_id}-0.txt"
    # URL secundaria (redirect oficial, puede devolver HTML en algunos casos)
    URL2="https://www.gutenberg.org/ebooks/${book_id}.txt.utf-8"
    # URL terciaria (sin sufijo -0, libros mas antiguos)
    URL3="https://www.gutenberg.org/files/${book_id}/${book_id}.txt"

    DESCARGADO=false
    for URL in "$URL1" "$URL2" "$URL3"; do
        HTTP_CODE=$(curl -s -L --max-time 30 -A "Mozilla/5.0" \
                        -w "%{http_code}" -o "$OUT" "$URL" 2>/dev/null)
        # Verificar que descargamos texto real (no HTML de error)
        if [ "$HTTP_CODE" = "200" ] && [ -s "$OUT" ]; then
            # Comprobar que no es una pagina HTML (los libros de texto no empiezan con <!DOCTYPE)
            FIRST=$(head -c 15 "$OUT" 2>/dev/null)
            if echo "$FIRST" | grep -qi "<!doctype\|<html"; then
                rm -f "$OUT"
            else
                DESCARGADO=true
                break
            fi
        else
            rm -f "$OUT"
        fi
    done

    if $DESCARGADO; then
        SIZE=$(wc -c < "$OUT" | tr -d ' ')
        echo -e "${GREEN}OK${NC} (${SIZE} bytes)"
        OK=$((OK + 1))
    else
        echo -e "${RED}FALLO${NC} (no hay version .txt disponible)"
        FAIL=$((FAIL + 1))
    fi

    sleep "$DELAY"
done < "$IDS_FILE"

echo ""
echo -e "${YELLOW}[3/3] Resultado de la descarga:${NC}"
echo "      Descargados: $OK / $TOTAL"
echo "      Fallidos:    $FAIL / $TOTAL"
echo "      Directorio:  $OUTPUT_DIR/"
echo ""

if [ "$OK" -eq 0 ]; then
    echo -e "${RED}ERROR: No se descargo ningun libro.${NC}"
    exit 1
fi

# ─────────────────────────────────────────────────────────────────────────────
# Estadisticas del corpus descargado
# ─────────────────────────────────────────────────────────────────────────────
echo -e "${YELLOW}=== Estadisticas del corpus ===${NC}"
echo -n "Libros descargados: "; ls "$OUTPUT_DIR"/*.txt 2>/dev/null | wc -l | tr -d ' '
echo -n "Tamano total:       "; du -sh "$OUTPUT_DIR" | cut -f1
echo -n "Libro mas grande:   "; ls -lS "$OUTPUT_DIR"/*.txt 2>/dev/null | head -1 | awk '{print $NF, $5, "bytes"}'
echo -n "Libro mas pequeno:  "; ls -lS "$OUTPUT_DIR"/*.txt 2>/dev/null | tail -1 | awk '{print $NF, $5, "bytes"}'
echo ""
echo -e "${GREEN}Listo. Para comprimir el corpus:${NC}"
echo "  cd serial     && ./compress  ../gutenberg_books ../gutenberg_serial.bin"
echo "  cd parallel   && ./compress  ../gutenberg_books ../gutenberg_parallel.bin"
echo "  cd concurrent && ./compress  ../gutenberg_books ../gutenberg_concurrent.bin"
echo ""
echo "O directamente:"
echo "  bash test_gutenberg.sh"
