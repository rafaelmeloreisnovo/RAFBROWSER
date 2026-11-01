#!/bin/sh
# BrowserRaf RAFA-SCAN V1: read-only, non-root diagnostic for Android/Termux.
# No persistent logs, changes to DNS, root access, packet interception, or TLS bypass.
set -u

usage() {
    printf '%s\n' 'Uso: sh tools/rafascan_network.sh [--offline|--active|--help]' \
      '  --offline  (padrao): somente configuracoes locais; zero requisicoes de rede.' \
      '  --active   : tres conexoes HTTPS (sistema, DoH, IP direto) e ping opcional.' \
      '  --help     : esta ajuda.' \
      'Consultas ativas: example.com, dns.google e one.one.one.one (1.1.1.1).'
}
mode=offline
case "${1:-}" in
    ''|--offline) ;;
    --active) mode=active ;;
    --help|-h) usage; exit 0 ;;
    *) usage >&2; exit 2 ;;
esac
if [ "$#" -gt 1 ]; then usage >&2; exit 2; fi

available() { command -v "$1" >/dev/null 2>&1; }
read_settings() {
    if available settings; then settings get global "$1" 2>/dev/null || :; fi
}
print_flag() { printf '%s: %s\n' "$1" "$2"; }

printf 'RAFA-SCAN V1 | modo=%s | nenhuma alteracao de sistema\n' "$mode"
printf '%s\n' 'ESCOPO: observa esta sessao/app; outros apps, roteadores e redes externas nao sao auditados.'

pdns=$(read_settings private_dns_mode)
case "$pdns" in
    hostname)
        host=$(read_settings private_dns_specifier)
        [ -n "$host" ] && [ "$host" != null ] || host='TOKEN_VAZIO_UNOBSERVED'
        print_flag 'DNS_PRIVADO' "CONFIGURADO (modo estrito; servidor=$host; nao validado)" ;;
    opportunistic) print_flag 'DNS_PRIVADO' 'ATENCAO (automatico pode permitir DNS sem criptografia)' ;;
    off) print_flag 'DNS_PRIVADO' 'ATENCAO (desativado)' ;;
    *) print_flag 'DNS_PRIVADO' 'TOKEN_VAZIO_UNOBSERVED (sem permissao ou API indisponivel)' ;;
esac

if available getprop; then
    d1=$(getprop net.dns1 2>/dev/null || :)
    d2=$(getprop net.dns2 2>/dev/null || :)
    if [ -n "$d1$d2" ]; then
        print_flag 'DNS_LEGADO' "observado (nao comprova resolvedor ativo): $d1 $d2"
    else
        print_flag 'DNS_LEGADO' 'TOKEN_VAZIO_UNOBSERVED (Android moderno pode ocultar propriedades)'
    fi
else
    print_flag 'DNS_LEGADO' 'TOKEN_VAZIO_UNOBSERVED (getprop indisponivel)'
fi

if [ -n "${HTTP_PROXY:-}${HTTPS_PROXY:-}${ALL_PROXY:-}${http_proxy:-}${https_proxy:-}${all_proxy:-}" ]; then
    print_flag 'PROXY_AMBIENTE' 'OBSERVADO (variavel definida; destino/valor omitido por privacidade)'
else
    print_flag 'PROXY_AMBIENTE' 'NAO_OBSERVADO (nao exclui proxy do Android ou da rede)'
fi
ntp_source=$(read_settings ntp_server)
case "$ntp_source" in
    ''|null) print_flag 'NTP_CONFIG' 'TOKEN_VAZIO_UNOBSERVED (nao diagnostica rotas)' ;;
    *) print_flag 'NTP_CONFIG' 'configurado (nao prova sincronizacao nem trajeto de pacotes)' ;;
esac
sys_proxy=$(read_settings http_proxy)
case "$sys_proxy" in
    ''|null|:0) print_flag 'PROXY_ANDROID' 'NAO_OBSERVADO ou indisponivel' ;;
    *) print_flag 'PROXY_ANDROID' 'OBSERVADO (valor omitido por privacidade)' ;;
esac

if available ip; then
    route=$(ip -4 route get 1.1.1.1 2>/dev/null || :)
    if [ -n "$route" ]; then
        print_flag 'ROTA_IPV4' 'OBSERVADA (consulta local; nao demonstra caminho externo)'
        iface=$(printf '%s\n' "$route" | sed -n 's/.* dev \([^ ]*\).*/\1/p' | head -n 1)
        [ -z "$iface" ] || print_flag 'INTERFACE' "$iface"
    else
        print_flag 'ROTA_IPV4' 'TOKEN_VAZIO_UNOBSERVED (permissao, utilitario ou rede)'
    fi
    links=$(ip -o link show 2>/dev/null || :)
    if [ -n "$links" ]; then
        if printf '%s\n' "$links" | grep -Eiq '(^|[ :])(tun[0-9]*|ppp[0-9]*|wg[0-9]*|tap[0-9]*)([ :]|$)'; then
            print_flag 'VPN_INTERFACE' 'INDICIO de interface virtual (nao prova tunel nem interceptacao)'
        else
            print_flag 'VPN_INTERFACE' 'NAO_OBSERVADO (nao exclui VPN)'
        fi
    else
        print_flag 'VPN_INTERFACE' 'TOKEN_VAZIO_UNOBSERVED'
    fi
else
    print_flag 'ROTA_IPV4' 'TOKEN_VAZIO_UNOBSERVED (ip indisponivel)'
    print_flag 'VPN_INTERFACE' 'TOKEN_VAZIO_UNOBSERVED'
fi

if [ "$mode" = offline ]; then
    printf '%s\n' 'NETWORK_PROBES: NOT_RUN (modo offline; nenhum ping/DNS externo/HTTPS solicitado)'
    exit 0
fi

printf '%s\n' 'CONSENTIMENTO: --active pediu 3 probes HTTPS e ping opcional; provedores veem IP de origem.'
if ! available curl; then
    printf '%s\n' 'NETWORK_PROBES: TOKEN_VAZIO_NOT_RUN (curl nao instalado)'
    exit 0
fi

probe() {
    if [ "$1" = direct ]; then
        curl --silent --show-error --noproxy '*' --proto '=https' \
             --connect-timeout 4 --max-time 10 --max-redirs 0 \
             --resolve 'one.one.one.one:443:1.1.1.1' \
             --output /dev/null \
             --write-out 'http=%{http_code} ip=%{remote_ip} tls=%{ssl_verify_result} dns_s=%{time_namelookup} connect_s=%{time_connect} handshake_s=%{time_appconnect} total_s=%{time_total}' \
             'https://one.one.one.one/' 2>/dev/null
    elif [ "$1" = doh ]; then
        curl --silent --show-error --noproxy '*' --proto '=https' \
             --connect-timeout 4 --max-time 10 --max-redirs 0 \
             --doh-url 'https://dns.google/dns-query' \
             --output /dev/null \
             --write-out 'http=%{http_code} ip=%{remote_ip} tls=%{ssl_verify_result} dns_s=%{time_namelookup} connect_s=%{time_connect} handshake_s=%{time_appconnect} total_s=%{time_total}' \
             'https://example.com/' 2>/dev/null
    else
        curl --silent --show-error --noproxy '*' --proto '=https' \
             --connect-timeout 4 --max-time 10 --max-redirs 0 \
             --output /dev/null \
             --write-out 'http=%{http_code} ip=%{remote_ip} tls=%{ssl_verify_result} dns_s=%{time_namelookup} connect_s=%{time_connect} handshake_s=%{time_appconnect} total_s=%{time_total}' \
             'https://example.com/' 2>/dev/null
    fi
}

sys_verified=0
if sys_result=$(probe system); then
    case " $sys_result " in
        *' tls=0 '*) sys_verified=1; print_flag 'HTTPS_DNS_SISTEMA' "TLS_VALIDADO $sys_result" ;;
        *) print_flag 'HTTPS_DNS_SISTEMA' "INCONCLUSIVO (status TLS anormal): $sys_result" ;;
    esac
else
    print_flag 'HTTPS_DNS_SISTEMA' 'FALHA (causa pode ser rede, certificado, relogio ou filtragem; nao prova ataque)'
fi

doh_verified=0
if curl --help all 2>/dev/null | grep -q -- '--doh-url'; then
    if doh_result=$(probe doh); then
        case " $doh_result " in
            *' tls=0 '*) doh_verified=1; print_flag 'HTTPS_DNS_DOH' "TLS_VALIDADO $doh_result" ;;
            *) print_flag 'HTTPS_DNS_DOH' "INCONCLUSIVO (status TLS anormal): $doh_result" ;;
        esac
    else
        print_flag 'HTTPS_DNS_DOH' 'FALHA (rede, suporte DoH ou TLS; nao prova desvio)'
    fi
else
    print_flag 'HTTPS_DNS_DOH' 'TOKEN_VAZIO_NOT_RUN (curl sem --doh-url)'
fi

if direct_result=$(probe direct); then
    case " $direct_result " in
        *' tls=0 '*) print_flag 'HTTPS_IP_DIRETO' "TLS_VALIDADO $direct_result" ;;
        *) print_flag 'HTTPS_IP_DIRETO' "INCONCLUSIVO (status TLS anormal): $direct_result" ;;
    esac
else
    print_flag 'HTTPS_IP_DIRETO' 'FALHA (certificado/rede/endpoint; nao prova ataque)'
fi
if available ping; then
    ping_summary=$(ping -c 3 -W 2 1.1.1.1 2>/dev/null | grep -E 'packets transmitted|packet loss|rtt |round-trip ' || :)
    if [ -n "$ping_summary" ]; then
        print_flag 'PING_ICMP' "$ping_summary"
    else
        print_flag 'PING_ICMP' 'TOKEN_VAZIO_UNMEASURED (ICMP pode ser bloqueado)'
    fi
else
    print_flag 'PING_ICMP' 'TOKEN_VAZIO_NOT_RUN (ping indisponivel)'
fi

if [ "$sys_verified" -eq 1 ] && [ "$doh_verified" -eq 1 ]; then
    sip=$(printf '%s\n' "$sys_result" | sed -n 's/.* ip=\([^ ]*\) tls=.*/\1/p')
    dip=$(printf '%s\n' "$doh_result" | sed -n 's/.* ip=\([^ ]*\) tls=.*/\1/p')
    if [ -n "$sip" ] && [ -n "$dip" ]; then
        if [ "$sip" = "$dip" ]; then
            print_flag 'DNS_COMPARACAO' 'IP_COINCIDENTE (nao prova ausencia de desvio)'
        else
            print_flag 'DNS_COMPARACAO' 'IP_DIVERGENTE_NAO_CONCLUSIVO (CDN/anycast podem explicar)'
        fi
    else
        print_flag 'DNS_COMPARACAO' 'TOKEN_VAZIO_UNMEASURED'
    fi
else
    print_flag 'DNS_COMPARACAO' 'TOKEN_VAZIO_UNMEASURED'
fi
printf '%s\n' 'INTERPRETACAO: IPs diferentes podem ser CDN/anycast; RTT nao mede distancia; TLS valido nao prova ausencia de malware.'
printf '%s\n' 'RESULTADO: observacoes locais, NAO auditoria antivirus completa.'
