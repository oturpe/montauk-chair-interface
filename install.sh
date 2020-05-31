echo "Asennetaan riippuvuudet."
dnf --assumeyes install make gcc alsa-lib-devel libgpiod-devel

echo "Konfiguroidaan systemd."
cp systemd/montauk-chair-interface.service /lib/systemd/system/
systemctl daemon-reload
systemctl enable montauk-chair-interface.service

echo "Konfiguraatio valmis. Käynnistä laite uudelleen, jotta asetukset astuvat voimaan."
