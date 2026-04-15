#!/bin/bash
# test_all.sh — Compila y prueba las tres versiones (serial, parallel, concurrent)
# Uso: bash test_all.sh
# Debe ejecutarse desde la raiz del proyecto (proyecto0/)

set -e  # salir si cualquier comando falla

PROJECT_ROOT="$(cd "$(dirname "$0")" && pwd)"
TEST_INPUT="$PROJECT_ROOT/test_input"
OUT_DIR="$PROJECT_ROOT/test_output"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # Sin color

echo "========================================================"
echo "  TEST COMPLETO: Huffman Serial / Parallel / Concurrent"
echo "========================================================"
echo ""

# ── Limpiar salidas anteriores ─────────────────────────────────────────────
rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"

# ── Verificar que existe test_input ───────────────────────────────────────
if [ ! -d "$TEST_INPUT" ]; then
    echo -e "${RED}ERROR: No existe test_input/. Crea los archivos de prueba primero.${NC}"
    exit 1
fi

echo -e "${YELLOW}Archivos de entrada:${NC}"
find "$TEST_INPUT" -type f | sort
echo ""

# ═══════════════════════════════════════════════════════════════════════════
# COMPILAR
# ═══════════════════════════════════════════════════════════════════════════

echo -e "${YELLOW}=== Compilando Serial ===${NC}"
cd "$PROJECT_ROOT/serial" && make clean -s && make -s
echo -e "${GREEN}Serial OK${NC}"

echo -e "${YELLOW}=== Compilando Parallel (fork) ===${NC}"
cd "$PROJECT_ROOT/parallel" && make clean -s && make -s
echo -e "${GREEN}Parallel OK${NC}"

echo -e "${YELLOW}=== Compilando Concurrent (pthread) ===${NC}"
cd "$PROJECT_ROOT/concurrent" && make clean -s && make -s
echo -e "${GREEN}Concurrent OK${NC}"

echo ""

# ═══════════════════════════════════════════════════════════════════════════
# SERIAL
# ═══════════════════════════════════════════════════════════════════════════
echo -e "${YELLOW}=== SERIAL: Comprimir ===${NC}"
mkdir -p "$OUT_DIR/serial"
"$PROJECT_ROOT/serial/compress" "$TEST_INPUT" "$OUT_DIR/serial/resultado.bin"

echo ""
echo -e "${YELLOW}=== SERIAL: Descomprimir ===${NC}"
"$PROJECT_ROOT/serial/decompress" "$OUT_DIR/serial/resultado.bin" "$OUT_DIR/serial/recuperado"

echo ""

# ═══════════════════════════════════════════════════════════════════════════
# PARALLEL (fork)
# ═══════════════════════════════════════════════════════════════════════════
echo -e "${YELLOW}=== PARALLEL (fork): Comprimir ===${NC}"
mkdir -p "$OUT_DIR/parallel"
"$PROJECT_ROOT/parallel/compress" "$TEST_INPUT" "$OUT_DIR/parallel/resultado.bin"

echo ""
echo -e "${YELLOW}=== PARALLEL (fork): Descomprimir ===${NC}"
"$PROJECT_ROOT/parallel/decompress" "$OUT_DIR/parallel/resultado.bin" "$OUT_DIR/parallel/recuperado"

echo ""

# ═══════════════════════════════════════════════════════════════════════════
# CONCURRENT (pthread)
# ═══════════════════════════════════════════════════════════════════════════
echo -e "${YELLOW}=== CONCURRENT (pthread): Comprimir ===${NC}"
mkdir -p "$OUT_DIR/concurrent"
"$PROJECT_ROOT/concurrent/compress" "$TEST_INPUT" "$OUT_DIR/concurrent/resultado.bin"

echo ""
echo -e "${YELLOW}=== CONCURRENT (pthread): Descomprimir ===${NC}"
"$PROJECT_ROOT/concurrent/decompress" "$OUT_DIR/concurrent/resultado.bin" "$OUT_DIR/concurrent/recuperado"

echo ""

# ═══════════════════════════════════════════════════════════════════════════
# VERIFICACION: los tres resultados deben ser identicos al input
# ═══════════════════════════════════════════════════════════════════════════
echo -e "${YELLOW}=== Verificando correctitud ===${NC}"

PASS=true

echo -n "Serial vs input:     "
if diff -r --brief "$TEST_INPUT" "$OUT_DIR/serial/recuperado" > /dev/null 2>&1; then
    echo -e "${GREEN}IGUAL${NC}"
else
    echo -e "${RED}DIFERENTE${NC}"
    diff -r "$TEST_INPUT" "$OUT_DIR/serial/recuperado"
    PASS=false
fi

echo -n "Parallel vs input:   "
if diff -r --brief "$TEST_INPUT" "$OUT_DIR/parallel/recuperado" > /dev/null 2>&1; then
    echo -e "${GREEN}IGUAL${NC}"
else
    echo -e "${RED}DIFERENTE${NC}"
    diff -r "$TEST_INPUT" "$OUT_DIR/parallel/recuperado"
    PASS=false
fi

echo -n "Concurrent vs input: "
if diff -r --brief "$TEST_INPUT" "$OUT_DIR/concurrent/recuperado" > /dev/null 2>&1; then
    echo -e "${GREEN}IGUAL${NC}"
else
    echo -e "${RED}DIFERENTE${NC}"
    diff -r "$TEST_INPUT" "$OUT_DIR/concurrent/recuperado"
    PASS=false
fi

echo ""
echo -e "${YELLOW}=== Tamaños de archivos .bin ===${NC}"
echo -n "Serial .bin:     "; wc -c < "$OUT_DIR/serial/resultado.bin"
echo -n "Parallel .bin:   "; wc -c < "$OUT_DIR/parallel/resultado.bin"
echo -n "Concurrent .bin: "; wc -c < "$OUT_DIR/concurrent/resultado.bin"

echo ""
if $PASS; then
    echo -e "${GREEN}========================================================"
    echo -e "  TODOS LOS TESTS PASARON"
    echo -e "========================================================${NC}"
else
    echo -e "${RED}========================================================"
    echo -e "  ALGUNOS TESTS FALLARON"
    echo -e "========================================================${NC}"
    exit 1
fi
