dnf --assumeyes install make gcc alsa-lib-devel libgpiod-devel
cp systemd/montauk-chair-interface.service /lib/systemd/system/
systemctl daemon-reload
systemctl enable montauk-chair-interface.service
