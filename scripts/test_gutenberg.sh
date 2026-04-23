#!/bin/bash
# test_gutenberg.sh
# Prueba las tres versiones con el corpus Gutenberg descargado.
# Ejecutar DESPUES de download_gutenberg.sh
#
# Uso:
#   bash test_gutenberg.sh

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BOOKS_DIR="$PROJECT_ROOT/gutenberg_books"
OUT_DIR="$PROJECT_ROOT/gutenberg_output"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

echo "========================================================"
echo "  TEST GUTENBERG: Serial / Parallel / Concurrent"
echo "========================================================"

if [ ! -d "$BOOKS_DIR" ] || [ -z "$(ls "$BOOKS_DIR"/*.txt 2>/dev/null)" ]; then
    echo -e "${RED}ERROR: No hay libros en gutenberg_books/.${NC}"
    echo "Ejecuta primero: bash download_gutenberg.sh"
    exit 1
fi

BOOK_COUNT=$(ls "$BOOKS_DIR"/*.txt | wc -l | tr -d ' ')
TOTAL_SIZE=$(du -sh "$BOOKS_DIR" | cut -f1)
echo -e "Corpus: ${CYAN}$BOOK_COUNT libros${NC}, ${CYAN}$TOTAL_SIZE${NC} en total"
echo ""

rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"

# ─── Compilar ────────────────────────────────────────────────────────────────
echo -e "${YELLOW}Compilando...${NC}"
(cd "$PROJECT_ROOT/serial"     && make -s)
(cd "$PROJECT_ROOT/parallel"   && make -s)
(cd "$PROJECT_ROOT/concurrent" && make -s)
echo -e "${GREEN}Compilacion OK${NC}"
echo ""

# ─── Funcion de prueba ────────────────────────────────────────────────────────
run_test() {
    local VERSION=$1
    local BIN_PATH="$PROJECT_ROOT/$VERSION"
    local OUTPUT_BIN="$OUT_DIR/${VERSION}.bin"
    local OUTPUT_DIR="$OUT_DIR/${VERSION}_recuperado"

    echo -e "${YELLOW}=== $VERSION ===${NC}"

    # Comprimir
    echo -n "  Comprimiendo $BOOK_COUNT libros... "
    TIME_OUTPUT=$("$BIN_PATH/compress" "$BOOKS_DIR" "$OUTPUT_BIN" 2>&1)
    COMPRESS_TIME=$(echo "$TIME_OUTPUT" | grep "Tiempo" | grep -oE '[0-9]+ ms')
    BIN_SIZE=$(wc -c < "$OUTPUT_BIN" | tr -d ' ')
    ORIG_SIZE=$(du -sb "$BOOKS_DIR" 2>/dev/null | cut -f1 || du -sk "$BOOKS_DIR" | awk '{print $1*1024}')
    echo -e "${GREEN}OK${NC} — $COMPRESS_TIME — .bin: $(numfmt --to=iec $BIN_SIZE 2>/dev/null || echo "${BIN_SIZE} bytes")"

    # Descomprimir
    echo -n "  Descomprimiendo... "
    mkdir -p "$OUTPUT_DIR"
    TIME_OUTPUT=$("$BIN_PATH/decompress" "$OUTPUT_BIN" "$OUTPUT_DIR" 2>&1)
    DECOMPRESS_TIME=$(echo "$TIME_OUTPUT" | grep "Tiempo" | grep -oE '[0-9]+ ms')
    echo -e "${GREEN}OK${NC} — $DECOMPRESS_TIME"

    # Verificar
    echo -n "  Verificando integridad... "
    if diff -r --brief "$BOOKS_DIR" "$OUTPUT_DIR" > /dev/null 2>&1; then
        echo -e "${GREEN}IGUAL${NC}"
        RESULT="PASS"
    else
        echo -e "${RED}DIFERENTE${NC}"
        diff -r --brief "$BOOKS_DIR" "$OUTPUT_DIR" | head -5
        RESULT="FAIL"
    fi

    echo -e "  ${CYAN}Compress: $COMPRESS_TIME  |  Decompress: $DECOMPRESS_TIME  |  $RESULT${NC}"
    echo ""
}

run_test "serial"
run_test "parallel"
run_test "concurrent"

# ─── Comparar tiempos ─────────────────────────────────────────────────────────
echo -e "${YELLOW}=== Comparacion de archivos .bin ===${NC}"
echo -n "  serial.bin:     "; wc -c < "$OUT_DIR/serial.bin" | tr -d ' '; echo " bytes"
echo -n "  parallel.bin:   "; wc -c < "$OUT_DIR/parallel.bin" | tr -d ' '; echo " bytes"
echo -n "  concurrent.bin: "; wc -c < "$OUT_DIR/concurrent.bin" | tr -d ' '; echo " bytes"
echo ""

# Calcular ratio de compresion
BIN_SIZE=$(wc -c < "$OUT_DIR/serial.bin" | tr -d ' ')
ORIG_BYTES=$(find "$BOOKS_DIR" -name "*.txt" -exec wc -c {} + | tail -1 | awk '{print $1}')
RATIO=$(awk "BEGIN {printf \"%.1f\", ($BIN_SIZE / $ORIG_BYTES) * 100}")
echo -e "  Ratio de compresion: ${CYAN}${RATIO}%${NC} del original"
echo ""
echo -e "${GREEN}Test completado. Los tres .bin deben tener el mismo tamano.${NC}"
