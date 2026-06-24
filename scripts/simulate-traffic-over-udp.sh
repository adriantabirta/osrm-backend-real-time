#!/bin/bash

# ============================================================
#  simulate_traffic.sh
#  Simulare trafic live — coordonate 100% aleatorii pe
#  bounding-box-ul orașului Chișinău, viteze realiste.
#
#  Utilizare:
#    ./simulate_traffic.sh [IP] [PORT] [INTERVAL_SEC] [NR_VEHICULE]
#
#  Exemple:
#    ./simulate_traffic.sh                        # 127.0.0.1:9900, 1s, 1 vehicul
#    ./simulate_traffic.sh 192.168.1.5 9900 0.5  # interval 0.5s
#    ./simulate_traffic.sh 127.0.0.1 9900 1 5    # 5 vehicule simultan
# ============================================================

IP="${1:-127.0.0.1}"
PORT="${2:-9900}"
INTERVAL="${3:-1}"
NR_VEHICULE="${4:-1}"

# ---- Bounding box Chișinău (acoperă tot orașul + suburbii) ----
LAT_MIN="46.9200"
LAT_MAX="47.0900"
LON_MIN="28.7800"
LON_MAX="29.0000"

# ---- Zone urbane cu limite de viteză diferite (lat_min lat_max lon_min lon_max spd_min spd_max) ----
# Centru / bulevarde principale  → 30-60 km/h
# Cartiere rezidențiale          → 20-50 km/h
# Artere principale (Dacia etc)  → 40-80 km/h
# Periferiq / șosele ieșire      → 60-110 km/h
ZONE=(
  "47.0100 47.0500 28.8400 28.9000 30 60"   # Centru
  "46.9800 47.0200 28.8600 28.9200 20 50"   # Botanica / Ciocana
  "47.0300 47.0700 28.8200 28.8800 25 55"   # Râșcani / Ciocana nord
  "47.0000 47.0400 28.9000 28.9600 30 60"   # Buiucani
  "46.9200 46.9900 28.8000 28.9000 60 110"  # Șosea sud (ieșire Hâncești)
  "47.0600 47.0900 28.8000 28.9000 60 100"  # Șosea nord (ieșire Orhei)
  "47.0000 47.0600 28.7800 28.8400 50 90"   # Șosea vest (ieșire Leușeni)
  "47.0000 47.0500 28.9400 29.0000 50 90"   # Șosea est
)

# ---- Tipuri de vehicule (ID coord — poate fi ID senzor / vehicul) ----
# Fiecare vehicul primește un ID fix între 10 și 99
rand_int() {
  # $1=min $2=max  →  număr întreg aleatoriu în interval
  awk -v mn="$1" -v mx="$2" -v seed="$RANDOM$RANDOM" \
    'BEGIN { srand(seed); print int(mn + rand()*(mx-mn+1)) }'
}

rand_float() {
  # $1=min $2=max cu 6 zecimale
  awk -v mn="$1" -v mx="$2" -v seed="$RANDOM$RANDOM" \
    'BEGIN { srand(seed); printf "%.6f", mn + rand()*(mx-mn) }'
}

# Alege o zonă aleatorie și generează coord + viteză în ea
gen_point() {
  local NZ=${#ZONE[@]}
  local ZI
  ZI=$(rand_int 0 $((NZ - 1)))
  local Z="${ZONE[$ZI]}"
  read -r zlat_min zlat_max zlon_min zlon_max zspd_min zspd_max <<< "$Z"

  LAT=$(rand_float "$zlat_min" "$zlat_max")
  LON=$(rand_float "$zlon_min" "$zlon_max")
  SPEED=$(rand_int "$zspd_min" "$zspd_max")
}

# ---- Funcție handler Ctrl+C ----
RUNNING=true
trap 'echo -e "\n\033[1;33m[STOP] Simulare oprită.\033[0m"; RUNNING=false; exit 0' INT TERM

# ---- Banner ----
echo ""
echo -e "\033[1;36m╔══════════════════════════════════════════════════╗\033[0m"
echo -e "\033[1;36m║       LIVE TRAFFIC SIMULATOR — Chișinău          ║\033[0m"
echo -e "\033[1;36m╠══════════════════════════════════════════════════╣\033[0m"
printf  "\033[1;36m║\033[0m  Target     : %-34s\033[1;36m║\033[0m\n" "$IP:$PORT"
printf  "\033[1;36m║\033[0m  Interval   : %-34s\033[1;36m║\033[0m\n" "${INTERVAL}s"
printf  "\033[1;36m║\033[0m  Vehicule   : %-34s\033[1;36m║\033[0m\n" "$NR_VEHICULE"
printf  "\033[1;36m║\033[0m  Zone       : %-34s\033[1;36m║\033[0m\n" "${#ZONE[@]} (centru/cartiere/șosele)"
echo -e "\033[1;36m╚══════════════════════════════════════════════════╝\033[0m"
echo -e "  Apasă \033[1;31mCtrl+C\033[0m pentru a opri.\n"

# ---- Generează ID-uri fixe pentru vehicule ----
declare -a VEH_IDS
for ((v=0; v<NR_VEHICULE; v++)); do
  VEH_IDS[$v]=$(rand_int 10 99)
done

# ---- Buclă principală ----
PKT=0
while true; do
  for ((v=0; v<NR_VEHICULE; v++)); do
    VID="${VEH_IDS[$v]}"

    gen_point   # setează LAT LON SPEED

    CMD="./build/live_traffic_publisher $IP $PORT $VID $LAT $LON $SPEED 0"

    # Culoare după viteză
    if   [ "$SPEED" -le 40 ]; then COLOR="\033[0;32m"   # verde  — mic
    elif [ "$SPEED" -le 70 ]; then COLOR="\033[0;33m"   # galben — mediu
    else                           COLOR="\033[0;31m"   # roșu   — mare
    fi

    PKT=$((PKT + 1))
    printf "${COLOR}[#%-5d]${COLOR} VEH=%2d | lat=%-11s lon=%-11s speed=%3d km/h\033[0m\n" \
      "$PKT" "$VID" "$LAT" "$LON" "$SPEED"

    eval "$CMD" 2>/dev/null

    # Interval mic între vehicule ca să nu se suprapună pe socket
    [ "$NR_VEHICULE" -gt 1 ] && sleep 0.05
  done

  sleep "$INTERVAL"
done
