#!/usr/bin/env bash
# CloudDeck host inventory (Phase 0). STRICTLY READ-ONLY.
#
# Collects everything docs/feasibility.md section 1 needs. It changes nothing:
# no package installs, no config writes, no service restarts. The only
# borderline step is `modprobe --dry-run`, which resolves module files without
# loading them; actually loading usbip modules is left to a separate, explicit step.
#
# Usage (as root, so firewall/journal/kernel info is complete):
#   sudo bash inventory.sh > inventory-$(hostname)-$(date +%F).txt 2>&1
# Review the output for anything sensitive before sharing it (it redacts
# obvious secrets but cannot know everything).

set -u
export LC_ALL=C

section() { printf '\n========== %s ==========\n' "$*"; }
run() { printf '\n$ %s\n' "$*"; timeout 20 bash -c "$*" 2>&1 | sed -E \
  -e 's/((pass(word)?|secret|token|api[_-]?key|credentials?)[[:space:]]*[=:][[:space:]]*)[^[:space:]]+/\1<redacted>/Ig' \
  -e 's/(Bearer|Basic|Token)[[:space:]]+[A-Za-z0-9._~+\/=-]{8,}/\1 <redacted>/g' \
  -e 's/([?&](token|key|sig|signature|auth)=)[^&[:space:]]+/\1<redacted>/Ig' \
  | cut -c1-400 | head -n 400; }

section "OS / Bazzite"
run 'cat /etc/os-release'
run 'rpm-ostree status --booted 2>/dev/null || rpm-ostree status'
run 'cat /usr/share/ublue-os/image-info.json 2>/dev/null'
run 'uname -a'
run 'cat /proc/cmdline'
run 'uptime; nproc; free -h; df -hT / /var /home 2>/dev/null'
run 'systemd-detect-virt; cat /sys/class/dmi/id/sys_vendor /sys/class/dmi/id/product_name 2>/dev/null'

section "Overlay / layered packages / kargs"
run 'rpm-ostree status --booted --json 2>/dev/null | grep -E "\"(requested-packages|requested-local-packages|packages|base-removals|base-local-replacements)\" :" | grep -v "ostree\.\(manifest\|container\)"'
run 'rpm-ostree kargs'

section "GPU and driver"
run 'lspci -nnk | grep -EA4 "VGA|3D|Display"'
run 'nvidia-smi 2>/dev/null || echo "no nvidia-smi"'
run 'cat /proc/driver/nvidia/version 2>/dev/null'
run 'ls -l /dev/dri/ /dev/dri/by-path 2>/dev/null'
run 'for c in /sys/class/drm/card*-*; do printf "%s status=%s enabled=%s\n" "$(basename $c)" "$(cat $c/status 2>/dev/null)" "$(cat $c/enabled 2>/dev/null)"; done'
run 'for c in /sys/class/drm/card*-*; do [ -s "$c/edid" ] && { echo "== $(basename $c) edid bytes: $(wc -c <$c/edid)"; }; done'
run 'lsmod | grep -Ei "nvidia|amdgpu|i915|xe|virtio_gpu|vkms|evdi|drm" '
run 'vainfo 2>/dev/null | head -40 || echo "no vainfo"'

section "Session (gamescope vs Plasma, Wayland vs X11)"
run 'loginctl list-sessions --no-legend'
run 'for s in $(loginctl list-sessions --no-legend | awk "{print \$1}"); do loginctl show-session "$s" -p Name -p Type -p Class -p Desktop -p State -p Remote -p Service; echo; done'
run 'ls -l /etc/sddm.conf.d/ 2>/dev/null; cat /etc/sddm.conf.d/*.conf 2>/dev/null | grep -Ev "^\s*(#|$)"'
run 'ls /usr/share/wayland-sessions /usr/share/xsessions 2>/dev/null'
run 'pgrep -a -f "gamescope|kwin_wayland|kwin_x11|Xwayland|plasmashell|steam " | cut -c1-200'
run 'command -v kscreen-doctor && for u in $(loginctl list-users --no-legend | awk "\$1>=1000{print \$2}"); do sudo -u "$u" XDG_RUNTIME_DIR=/run/user/$(id -u $u) WAYLAND_DISPLAY=wayland-0 kscreen-doctor -o 2>&1 | head -40; done'
run 'kwin_wayland --version 2>/dev/null; plasmashell --version 2>/dev/null; gamescope --version 2>&1 | head -2'

section "Sunshine"
run 'command -v sunshine; ls -l $(command -v sunshine) 2>/dev/null; sunshine --version 2>&1 | head -3'
run 'flatpak list --app 2>/dev/null | grep -i -E "sunshine|moonlight|apollo"'
run 'rpm -qa | grep -i sunshine'
run 'getcap $(readlink -f $(command -v sunshine) 2>/dev/null) 2>/dev/null'
run 'systemctl list-units --all --no-pager | grep -i sunshine; for u in $(loginctl list-users --no-legend | awk "\$1>=1000{print \$2}"); do echo "user=$u"; sudo -u "$u" XDG_RUNTIME_DIR=/run/user/$(id -u $u) systemctl --user list-units --all --no-pager 2>/dev/null | grep -i sunshine; done'
run 'systemctl cat sunshine 2>/dev/null; for u in $(loginctl list-users --no-legend | awk "\$1>=1000{print \$2}"); do sudo -u "$u" XDG_RUNTIME_DIR=/run/user/$(id -u $u) systemctl --user cat sunshine 2>/dev/null; done'
run 'pgrep -a sunshine'
run 'for d in /home/*/.config/sunshine /root/.config/sunshine /etc/sunshine; do [ -d "$d" ] && { echo "== $d"; ls -la "$d"; }; done'
# Config only (no state file: sunshine_state.json holds paired client certs).
run 'for f in /home/*/.config/sunshine/sunshine.conf /etc/sunshine/sunshine.conf; do [ -f "$f" ] && { echo "== $f"; grep -Ev "^\s*(#|$)" "$f"; }; done'
run 'for f in /home/*/.config/sunshine/apps.json; do [ -f "$f" ] && { echo "== $f"; head -c 3000 "$f"; }; done'
run 'for f in /home/*/.config/sunshine/sunshine_state.json; do [ -f "$f" ] && { echo "== $f (keys only)"; grep -oE "\"[a-z_]+\":" "$f" | sort | uniq -c; }; done'

section "Audio (PipeWire)"
run 'pipewire --version 2>/dev/null; wireplumber --version 2>/dev/null'
run 'for u in $(loginctl list-users --no-legend | awk "\$1>=1000{print \$2}"); do echo "user=$u"; sudo -u "$u" XDG_RUNTIME_DIR=/run/user/$(id -u $u) pactl info 2>&1 | head -20; sudo -u "$u" XDG_RUNTIME_DIR=/run/user/$(id -u $u) pactl list short sinks; sudo -u "$u" XDG_RUNTIME_DIR=/run/user/$(id -u $u) pactl list short sources; done'

section "USB/IP kernel support (no loading)"
run 'ls /lib/modules/$(uname -r)/kernel/drivers/usb/usbip/ 2>/dev/null || find /lib/modules/$(uname -r) -name "*usbip*" -o -name "vhci*"'
run 'for m in usbip-core vhci-hcd usbip-host; do modinfo -F filename $m 2>&1; modprobe --dry-run --verbose $m 2>&1; done'
run 'lsmod | grep -Ei "usbip|vhci"'
run 'command -v usbip usbipd; usbip version 2>&1'
run 'grep -E "CONFIG_USBIP|CONFIG_USB_VHCI|CONFIG_UINPUT|CONFIG_DRM_VKMS|CONFIG_SND_ALOOP" /boot/config-$(uname -r) /usr/lib/modules/$(uname -r)/config 2>/dev/null'
run 'ls -l /dev/uinput; getfacl /dev/uinput 2>/dev/null | tail -n +2'

section "Network and firewall"
run 'ip -br addr; ip route'
run 'cat /etc/resolv.conf | grep -v "^#"'
run 'curl -s --max-time 5 https://api.ipify.org; echo " (public IPv4 as seen from VM)"'
run 'firewall-cmd --state; firewall-cmd --get-active-zones; firewall-cmd --list-all-zones 2>/dev/null | grep -Ev "^\s*$"'
run 'firewall-cmd --direct --get-all-rules; firewall-cmd --list-rich-rules'
run 'nft list tables 2>/dev/null'
run 'nft list ruleset 2>/dev/null | grep -vE "^\s*$" | grep -E "table|chain|saddr|daddr|dport|accept|drop|reject" | grep -v "^\s*jump" | head -150'
run 'iptables -S 2>/dev/null | head -80'
run 'ss -tulpnH | sort -k5'

section "CloudDeck / provider agents (identify, do not touch)"
run 'systemctl list-units --type=service --state=running --no-pager --no-legend | cut -c1-140'
run 'systemctl list-unit-files --no-pager --no-legend | grep -viE "^(systemd|dbus|getty|user|sys-|dev-|proc-)" | grep -E "enabled|static" | cut -c1-120 | head -120'
run 'systemctl list-timers --all --no-pager | head -40'
run 'ls -la /opt /usr/local/bin /usr/local/sbin /etc/systemd/system 2>/dev/null'
run 'grep -rlsiE "clouddeck|cldk" /etc /opt /usr/local /var/lib 2>/dev/null | head -50'
run 'ps -eo pid,user,etimes,args --sort=pid | grep -iE "clouddeck|cldk|agent|webrtc|signal|cloud-init|qemu-ga|guest" | grep -v grep | cut -c1-200'
run 'cloud-init status --long 2>/dev/null; ls /var/lib/cloud/instance 2>/dev/null'
run 'journalctl -b --no-pager -o short-iso 2>/dev/null | grep -iE "clouddeck|cldk|firewall|allowlist|whitelist" | tail -60'
run 'crontab -l 2>/dev/null; ls /etc/cron.d 2>/dev/null'

section "SSH access"
run 'systemctl is-active sshd; grep -Ev "^\s*(#|$)" /etc/ssh/sshd_config /etc/ssh/sshd_config.d/*.conf 2>/dev/null | grep -iE "port|permitroot|password|pubkey"'

section "done"
date -Is
