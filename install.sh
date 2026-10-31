#!/usr/bin/env bash
set -Eeuo pipefail

# Sentinel installer. Install: ./install.sh  Uninstall: ./install.sh -r

PROJECT_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
WEB_DIR="$PROJECT_ROOT/web"
AGENT_DIR="$PROJECT_ROOT/agent"
VENV_DIR="$WEB_DIR/.venv"

AGENT_TARGET_NAME="sentinel-agent"
AGENT_BINARY="/usr/local/bin/sentinel-agent"
SENTINEL_DIR="/etc/sentinel"
AGENT_CONFIG="$SENTINEL_DIR/config.ini"
WEB_ENV="$SENTINEL_DIR/web.env"
DASHBOARD_USER_FILE="$SENTINEL_DIR/dashboard-user"
AGENT_STATE_DIR="/var/lib/sentinel"

AGENT_SERVICE="sentinel-agent.service"
WEB_SERVICE="sentinel-web.service"

die() {
  echo "ERROR: $*" >&2
  exit 1
}

get_run_user() {
  if [[ -n "${SUDO_USER:-}" && "$SUDO_USER" != "root" ]]; then
    printf '%s' "$SUDO_USER"
  else
    id -un
  fi
}

detect_distro_family() {
  if [[ -r /etc/os-release ]]; then
    . /etc/os-release
    case "${ID:-}${ID_LIKE:-}" in
    *debian* | *ubuntu*) echo "debian" ;;
    *fedora* | *rhel* | *centos*) echo "fedora" ;;
    *arch*) echo "arch" ;;
    *suse*) echo "suse" ;;
    *) echo "unknown" ;;
    esac
  else
    echo "unknown"
  fi
}

print_missing_deps_help() {
  local family="$1"
  shift
  local missing=("$@")

  echo
  echo "Missing required dependencies:"
  printf '  - %s\n' "${missing[@]}"
  echo
  echo "Install them with your distribution's package manager:"
  echo

  case "$family" in
  debian)
    cat <<'EOF'
  Debian / Ubuntu:
    sudo apt update
    sudo apt install build-essential cmake python3 python3-venv python3-pip ca-certificates

  If SELinux tooling is needed (rare on Debian):
    sudo apt install policycoreutils
EOF
    ;;
  fedora)
    cat <<'EOF'
  Fedora / RHEL / CentOS:
    sudo dnf install gcc make cmake python3 python3-pip ca-certificates policycoreutils-python-utils

  If python3-venv is a separate package on your release:
    sudo dnf install python3-venv
EOF
    ;;
  arch)
    cat <<'EOF'
  Arch:
    sudo pacman -S base-devel cmake python python-pip ca-certificates
EOF
    ;;
  suse)
    cat <<'EOF'
  openSUSE:
    sudo zypper install gcc make cmake python3 python3-pip ca-certificates
EOF
    ;;
  *)
    cat <<'EOF'
  Unknown distribution. Install equivalents of:
    build tools (gcc, make), cmake, python3 with venv and pip, ca-certificates

  On Fedora/RHEL, also install policycoreutils-python-utils for SELinux tooling.
EOF
    ;;
  esac
  echo
}

check_dependencies() {
  echo "Checking required dependencies..."
  local missing=()

  for cmd in sudo systemctl cmake make gcc python3 hostname; do
    command -v "$cmd" >/dev/null 2>&1 || missing+=("$cmd")
  done

  if command -v python3 >/dev/null 2>&1; then
    python3 -m venv --help >/dev/null 2>&1 ||
      missing+=("python3-venv (python venv module)")
    python3 -m pip --version >/dev/null 2>&1 ||
      missing+=("python3-pip")
  fi

  if command -v getenforce >/dev/null 2>&1 &&
    [[ "$(getenforce 2>/dev/null)" == "Enforcing" ]]; then
    command -v semanage >/dev/null 2>&1 ||
      echo "WARNING: SELinux is enforcing but 'semanage' is missing." \
        "The installer will fall back to chcon (not persistent across relabel)." >&2
  fi

  if [[ ${#missing[@]} -gt 0 ]]; then
    local family
    family="$(detect_distro_family)"
    print_missing_deps_help "$family" "${missing[@]}"
    die "Install the missing dependencies and re-run ./install.sh"
  fi

  echo "All required dependencies are satisfied."
  echo
}

fix_selinux_contexts() {
  command -v getenforce >/dev/null 2>&1 || return 0
  [[ "$(getenforce 2>/dev/null)" == "Enforcing" ]] || return 0

  echo "SELinux is enforcing; applying contexts..."

  if command -v semanage >/dev/null 2>&1; then
    sudo semanage fcontext -a -t bin_t "$VENV_DIR/bin(/.*)?" 2>/dev/null ||
      sudo semanage fcontext -m -t bin_t "$VENV_DIR/bin(/.*)?" 2>/dev/null ||
      true
    sudo semanage fcontext -a -t bin_t "$AGENT_BINARY" 2>/dev/null ||
      sudo semanage fcontext -m -t bin_t "$AGENT_BINARY" 2>/dev/null ||
      true
  else
    echo "WARNING: semanage not found; using chcon fallback (not persistent across relabel)."
  fi

  if command -v restorecon >/dev/null 2>&1; then
    sudo restorecon -R "$VENV_DIR/bin" 2>/dev/null || true
    sudo restorecon "$AGENT_BINARY" 2>/dev/null || true
  else
    sudo chcon -R -t bin_t "$VENV_DIR/bin" 2>/dev/null || true
    sudo chcon -t bin_t "$AGENT_BINARY" 2>/dev/null || true
  fi
}

uninstall_sentinel() {
  echo "=== Sentinel Uninstaller ==="
  echo
  echo "This removes the Sentinel agent and web services."
  echo "Firewall rules will not be changed."
  echo "Your project source code will be preserved."
  echo

  read -r -p "Continue with uninstallation? [y/N]: " confirm
  [[ "$confirm" =~ ^[Yy]$ ]] || {
    echo "Uninstallation cancelled."
    return 0
  }

  echo
  echo "Choose what to do with Sentinel's data:"
  echo "  1) Keep data"
  echo "  2) Delete data"
  echo

  read -r -p "Choose [1/2]: " data_choice
  case "$data_choice" in
  1) ;;
  2)
    echo
    echo "WARNING: Sentinel's stored data will be permanently deleted."
    read -r -p 'Type DELETE to confirm: ' delete_confirm
    [[ "$delete_confirm" == "DELETE" ]] || {
      echo "Uninstallation cancelled."
      return 0
    }
    ;;
  *) die "Invalid choice. Nothing was removed." ;;
  esac

  require_cmd() { command -v "$1" >/dev/null 2>&1 || die "Required command not found: $1"; }
  require_cmd sudo
  require_cmd systemctl

  echo
  echo "Stopping and disabling Sentinel services..."
  for service in "$AGENT_SERVICE" "$WEB_SERVICE"; do
    sudo systemctl disable --now "$service" 2>/dev/null || true
  done

  echo "Removing systemd service files..."
  sudo rm -f "/etc/systemd/system/$AGENT_SERVICE" "/etc/systemd/system/$WEB_SERVICE"
  sudo systemctl daemon-reload
  sudo systemctl reset-failed 2>/dev/null || true

  echo "Removing installed agent binary..."
  sudo rm -f "$AGENT_BINARY"

  rm -rf -- "$VENV_DIR"

  if [[ "$data_choice" == "2" ]]; then
    echo "Removing Sentinel configuration and stored data..."
    sudo rm -rf -- "$SENTINEL_DIR"
    sudo rm -rf -- "$AGENT_STATE_DIR"
    rm -f -- "$WEB_DIR/db.sqlite3"
    sudo rm -f -- /tmp/metrics.json
    find "$WEB_DIR" -type d -name __pycache__ -prune -exec rm -rf -- {} + 2>/dev/null || true
    echo "Sentinel data removed."
  else
    echo "Sentinel configuration and dashboard data preserved."
  fi

  echo
  echo "Uninstallation complete."
  echo "Project source code remains at: $PROJECT_ROOT"
}

if [[ "${1:-}" == "-r" ]]; then
  [[ "$#" -eq 1 ]] || die "Usage: $0 -r"
  uninstall_sentinel
  exit 0
fi
[[ "$#" -eq 0 ]] || die "Usage: $0 [-r]"

[[ "$(id -u)" -ne 0 ]] || die "Do not run this script as root. Run it as your normal user."

check_dependencies

[[ -d "$AGENT_DIR" ]] || die "Agent directory not found: $AGENT_DIR"
[[ -f "$AGENT_DIR/CMakeLists.txt" ]] || die "Missing $AGENT_DIR/CMakeLists.txt"
[[ -d "$WEB_DIR" ]] || die "Web directory not found: $WEB_DIR"
[[ -f "$WEB_DIR/manage.py" ]] || die "Missing $WEB_DIR/manage.py"

AGENT_SERVICE_SOURCE="$AGENT_DIR/systemd/$AGENT_SERVICE"
[[ -f "$AGENT_SERVICE_SOURCE" ]] || die "Missing agent service file: $AGENT_SERVICE_SOURCE"

RUN_USER="$(get_run_user)"
RUN_GROUP="$(id -gn "$RUN_USER")"
RUN_HOME="$(getent passwd "$RUN_USER" | cut -d: -f6)"
[[ -n "$RUN_HOME" && -d "$RUN_HOME" ]] || die "Cannot determine home for $RUN_USER"

echo "=== Sentinel Installer ==="
echo "Project: $PROJECT_ROOT"
echo "Service user: $RUN_USER"
echo

echo "Preparing installation directories..."
sudo install -d -m 755 "$SENTINEL_DIR"
sudo install -d -m 755 "$AGENT_STATE_DIR"

if [[ -f "$AGENT_DIR/config.ini" && ! -f "$AGENT_CONFIG" ]]; then
  sudo install -m 640 "$AGENT_DIR/config.ini" "$AGENT_CONFIG"
elif [[ ! -f "$AGENT_CONFIG" ]]; then
  echo "WARNING: No agent config at $AGENT_DIR/config.ini and no $AGENT_CONFIG"
fi

echo
echo "Building Sentinel agent..."
cmake -S "$AGENT_DIR" -B "$AGENT_DIR/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$AGENT_DIR/build" --parallel

BUILT_AGENT=""
while IFS= read -r -d '' candidate; do
  case "$candidate" in */CMakeFiles/*) continue ;; esac
  if [[ -x "$candidate" && ! -d "$candidate" ]] &&
    [[ "$(basename "$candidate")" == "$AGENT_TARGET_NAME" ]]; then
    BUILT_AGENT="$candidate"
    break
  fi
done < <(find "$AGENT_DIR/build" -maxdepth 4 -type f -executable -print0)

[[ -n "$BUILT_AGENT" ]] ||
  die "Built target '$AGENT_TARGET_NAME' not found under $AGENT_DIR/build. Check CMakeLists.txt."
echo "Built agent: $BUILT_AGENT"

sudo install -m 755 "$BUILT_AGENT" "$AGENT_BINARY"

if ! cmp -s "$BUILT_AGENT" "$AGENT_BINARY"; then
  die "Installed binary does not match built binary ($BUILT_AGENT vs $AGENT_BINARY)"
fi

sudo install -m 644 "$AGENT_SERVICE_SOURCE" "/etc/systemd/system/$AGENT_SERVICE"

echo
echo "Preparing Django environment..."

if [[ ! -x "$VENV_DIR/bin/python" ]]; then
  python3 -m venv "$VENV_DIR" || die "Failed to create venv. Ensure python3-venv is installed."
fi

DJANGO_PYTHON="$VENV_DIR/bin/python"
PIP="$VENV_DIR/bin/pip"

"$DJANGO_PYTHON" -m pip install --upgrade pip
[[ -f "$WEB_DIR/requirements.txt" ]] && "$PIP" install -r "$WEB_DIR/requirements.txt"
"$PIP" install Django gunicorn whitenoise
"$DJANGO_PYTHON" -c 'import django' || die "Django installation failed."

echo
echo "Configuring dashboard access..."

HOSTS=("localhost" "127.0.0.1" "$(hostname -s)" "$(hostname -f 2>/dev/null || true)")
while IFS= read -r ip; do [[ -n "$ip" ]] && HOSTS+=("$ip"); done < <(hostname -I 2>/dev/null | tr ' ' '\n')

echo "Detected hostnames and addresses:"
printf '  %s\n' "${HOSTS[@]}"

read -r -p "Additional hostnames/IPs (optional, comma-separated): " EXTRA_HOSTS
if [[ -n "$EXTRA_HOSTS" ]]; then
  IFS=',' read -r -a EXTRA_HOST_ARRAY <<<"$EXTRA_HOSTS"
  for host in "${EXTRA_HOST_ARRAY[@]}"; do
    host="${host//[[:space:]]/}"
    [[ -n "$host" ]] && HOSTS+=("$host")
  done
fi

declare -A SEEN_HOSTS=()
ALLOWED_HOSTS=""
for host in "${HOSTS[@]}"; do
  [[ -n "$host" ]] || continue
  [[ -n "${SEEN_HOSTS[$host]:-}" ]] && continue
  SEEN_HOSTS["$host"]=1
  [[ -n "$ALLOWED_HOSTS" ]] && ALLOWED_HOSTS+=","
  ALLOWED_HOSTS+="$host"
done

SECRET_KEY=""
if [[ -f "$WEB_ENV" ]]; then
  SECRET_KEY="$(sudo sed -n 's/^DJANGO_SECRET_KEY=//p' "$WEB_ENV" | head -n 1)"
fi
if [[ -z "$SECRET_KEY" ]]; then
  SECRET_KEY="$("$DJANGO_PYTHON" -c 'from django.core.management.utils import get_random_secret_key; print(get_random_secret_key())')"
fi

sudo touch "$WEB_ENV"
sudo chmod 600 "$WEB_ENV"
sudo tee "$WEB_ENV" >/dev/null <<EOF
DJANGO_SECRET_KEY=$SECRET_KEY
DJANGO_DEBUG=False
DJANGO_ALLOWED_HOSTS=$ALLOWED_HOSTS
EOF
sudo chmod 600 "$WEB_ENV"

export DJANGO_SECRET_KEY="$SECRET_KEY"
export DJANGO_DEBUG=False
export DJANGO_ALLOWED_HOSTS="$ALLOWED_HOSTS"

echo
echo "Configure the Sentinel dashboard login."
echo

while true; do
  read -r -p "Dashboard username: " DASHBOARD_USERNAME
  [[ -n "$DASHBOARD_USERNAME" ]] || {
    echo "Username cannot be empty."
    continue
  }
  break
done

while true; do
  read -r -s -p "Dashboard password: " DASHBOARD_PASSWORD
  echo
  [[ -n "$DASHBOARD_PASSWORD" ]] || {
    echo "Password cannot be empty."
    continue
  }
  read -r -s -p "Confirm password: " DASHBOARD_PASSWORD_CONFIRM
  echo
  [[ "$DASHBOARD_PASSWORD" == "$DASHBOARD_PASSWORD_CONFIRM" ]] && break
  echo "Passwords do not match. Try again."
done

OLD_USERNAME=""
[[ -f "$DASHBOARD_USER_FILE" ]] && OLD_USERNAME="$(sudo cat "$DASHBOARD_USER_FILE" 2>/dev/null || true)"

cd "$WEB_DIR"

echo
echo "Running database migrations..."
"$DJANGO_PYTHON" manage.py migrate --noinput

echo "Configuring dashboard account..."
SENTINEL_USERNAME="$DASHBOARD_USERNAME" \
  SENTINEL_PASSWORD="$DASHBOARD_PASSWORD" \
  SENTINEL_OLD_USERNAME="$OLD_USERNAME" \
  "$DJANGO_PYTHON" manage.py shell -c '
import os
from django.contrib.auth import get_user_model
User = get_user_model()
username = os.environ["SENTINEL_USERNAME"]
password = os.environ["SENTINEL_PASSWORD"]
old_username = os.environ.get("SENTINEL_OLD_USERNAME", "")
user = User.objects.filter(username=username).first()
if user is None and old_username and old_username != username:
    user = User.objects.filter(username=old_username).first()
    if user is not None:
        user.username = username
if user is None:
    user = User(username=username)
user.set_password(password)
user.is_active = True
user.is_staff = False
user.is_superuser = False
user.save()
if old_username and old_username != username:
    User.objects.filter(username=old_username).exclude(pk=user.pk).delete()
print("Dashboard account configured.")
'

printf '%s' "$DASHBOARD_USERNAME" | sudo tee "$DASHBOARD_USER_FILE" >/dev/null
sudo chmod 600 "$DASHBOARD_USER_FILE"
unset DASHBOARD_PASSWORD DASHBOARD_PASSWORD_CONFIRM

echo "Collecting static files..."
"$DJANGO_PYTHON" manage.py collectstatic --noinput

echo
echo "Configuring dashboard service..."
sudo tee "/etc/systemd/system/$WEB_SERVICE" >/dev/null <<EOF
[Unit]
Description=Sentinel Django Dashboard
After=network-online.target sentinel-agent.service
Wants=network-online.target

[Service]
Type=simple
User=$RUN_USER
Group=$RUN_GROUP
WorkingDirectory=$WEB_DIR
EnvironmentFile=$WEB_ENV
ExecStart=$DJANGO_PYTHON -m gunicorn --workers 2 --bind 0.0.0.0:8000 config.wsgi:application
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF
sudo chmod 644 "/etc/systemd/system/$WEB_SERVICE"

fix_selinux_contexts

echo
echo "Enabling and starting Sentinel services..."
sudo systemctl daemon-reload
sudo systemctl enable "$AGENT_SERVICE" "$WEB_SERVICE"
sudo systemctl restart "$AGENT_SERVICE"
sudo systemctl restart "$WEB_SERVICE"

sleep 2
FAILED=0
for svc in "$AGENT_SERVICE" "$WEB_SERVICE"; do
  if ! sudo systemctl is-active --quiet "$svc"; then
    echo
    echo "ERROR: $svc is not active. Recent journal:"
    sudo journalctl -u "$svc" -n 30 --no-pager || true
    FAILED=1
  fi
done
[[ "$FAILED" -eq 0 ]] || die "One or more services failed to start."

echo
echo "=== Installation complete ==="
echo
echo "Dashboard URLs:"
echo "  http://localhost:8000"
echo "  http://127.0.0.1:8000"
while IFS= read -r ip; do [[ -n "$ip" ]] && echo "  http://$ip:8000"; done < <(hostname -I 2>/dev/null | tr ' ' '\n')

echo
echo "Listening sockets:"
sudo ss -ltnp 2>/dev/null | grep ':8000' || true

echo
echo "Service status:"
sudo systemctl --no-pager --full status "$AGENT_SERVICE" || true
sudo systemctl --no-pager --full status "$WEB_SERVICE" || true

echo
echo "Firewall rules were not changed."
