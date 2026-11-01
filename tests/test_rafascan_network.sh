#!/bin/sh
# Hermetic offline tests; --active uses mock curl and ping only (no internet).
set -eu
base=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp/bin"
cat > "$tmp/bin/settings" <<'M'
#!/bin/sh
case "$3" in
private_dns_mode) echo hostname ;;
private_dns_specifier) echo dns.example.test ;;
ntp_server) echo time.example.test ;;
http_proxy) echo null ;;
*) echo null ;;
esac
M
cat > "$tmp/bin/getprop" <<'M'
#!/bin/sh
case "$1" in net.dns1) echo 192.0.2.53 ;; esac
M
cat > "$tmp/bin/ip" <<'M'
#!/bin/sh
case "$1 $2" in
'-4 route') echo '1.1.1.1 via 192.0.2.1 dev wlan0 src 192.0.2.10' ;;
'-o link') echo '1: lo: <LOOPBACK>'; echo '3: wlan0: <UP>' ;;
esac
M
cat > "$tmp/bin/curl" <<'M'
#!/bin/sh
if [ "${1:-}" = --help ]; then echo '--doh-url'; exit 0; fi
printf '%s\n' "$*" >> "$MOCK_CURL_LOG"
case " $* " in
    *' --resolve '*) printf 'http=200 ip=1.1.1.1 tls=0 dns_s=0.000 connect_s=0.055 handshake_s=0.099 total_s=0.110' ;;
    *' --doh-url '*)
        if [ "${MOCK_FAIL_DOH:-0}" = 1 ]; then exit 60; fi
        printf 'http=200 ip=203.0.113.9 tls=0 dns_s=0.016 connect_s=0.090 handshake_s=0.120 total_s=0.180'
        ;;
    *) printf 'http=200 ip=192.0.2.80 tls=0 dns_s=0.030 connect_s=0.070 handshake_s=0.130 total_s=0.200' ;;
esac
M
cat > "$tmp/bin/ping" <<'M'
#!/bin/sh
echo '3 packets transmitted, 3 received, 0% packet loss'
echo 'rtt min/avg/max/mdev = 4.00/6.00/8.00/2.00 ms'
M
chmod +x "$tmp/bin/"*
export MOCK_CURL_LOG="$tmp/curl.calls"
export PATH="$tmp/bin:$PATH"
unset HTTP_PROXY HTTPS_PROXY ALL_PROXY http_proxy https_proxy all_proxy || :
sh "$base/tools/rafascan_network.sh" --offline > "$tmp/offline.out"
grep -q 'DNS_PRIVADO: CONFIGURADO' "$tmp/offline.out"
grep -q 'NETWORK_PROBES: NOT_RUN' "$tmp/offline.out"
[ ! -f "$MOCK_CURL_LOG" ] || { echo 'FAIL network call offline'; exit 1; }
sh "$base/tools/rafascan_network.sh" --active > "$tmp/active.out"
grep -q 'HTTPS_DNS_SISTEMA: TLS_VALIDADO' "$tmp/active.out"
grep -q 'HTTPS_DNS_DOH: TLS_VALIDADO' "$tmp/active.out"
grep -q 'DNS_COMPARACAO: IP_DIVERGENTE_NAO_CONCLUSIVO' "$tmp/active.out"
grep -q 'HTTPS_IP_DIRETO: TLS_VALIDADO' "$tmp/active.out"
grep -q 'PING_ICMP: 3 packets transmitted' "$tmp/active.out"
[ "$(wc -l < "$MOCK_CURL_LOG")" -eq 3 ]
grep -q -- '--noproxy \*' "$MOCK_CURL_LOG"
grep -q -- '--proto =https' "$MOCK_CURL_LOG"
grep -q -- '--resolve one.one.one.one:443:1.1.1.1' "$MOCK_CURL_LOG"
if grep -E -- '(^| )(-k|--insecure|-L|--location)( |$)' "$MOCK_CURL_LOG"; then
    echo 'FAIL unsafe curl flags'; exit 1
fi
MOCK_FAIL_DOH=1 sh "$base/tools/rafascan_network.sh" --active > "$tmp/fail.out"
grep -q 'HTTPS_DNS_DOH: FALHA' "$tmp/fail.out"
grep -q 'DNS_COMPARACAO: TOKEN_VAZIO_UNMEASURED' "$tmp/fail.out"
if sh "$base/tools/rafascan_network.sh" --typo > /dev/null 2>&1; then
    echo 'FAIL accepted unknown option'; exit 1
fi
printf '%s\n' 'PASS_SCOPED RAFA-SCAN V1: passive, triple HTTPS mock, TLS fail, privacy, args (no network)'
