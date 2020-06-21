## Montauk chair interface
Montauk-aikakoneen ohjain Raspberry Pille. Tehty osaksi [Kokeellisen elektroniikan seuran][koelse]
[Montauk-aikakonetta][aikakone].

[koelse]: http://koelse.org
[aikakone]: https://oturpe.kapsi.fi/museum-of-paranormal-technology/exhibits/Montauk_Time_Machine.html

### Laitteisto
1. Raspberry Pi 3B+
2. Usb-äänikortti
3. Liitin Raspberry Pi:n nastaan 12

Paranormaalin tekniikan museon aikakoneessa nastan 12 ulostulo syötettiin signaaligeneraattoriin, jolle
Raspberry Pin 3,3 voltin ulostulo oli sopiva.

### Asentaminen
Raspberry Pin konfigurointi
1. Lataa Fedora 32 Server Edition aarch64
1. Asenna Fedora microSD-kortille:

    `sudo arm-image-installer --image=Fedora-Server-32-1.6.aarch64.raw.xz --target=rpi3 --media=LAITE --resizefs`

2. Laita kortti laitteeseen, vastaan asennusohjelman kysymyksiin:
    1. *Kieli*: _Suomi_
    2. *Aikavyöhyke*: _Europe/Helsinki_
    3. *Pääkäyttäjän salasana*: aseta haluamasi
3. Kirjaudu sisään pääkäyttäjänä
4. Aseta suomalainen näppäimistöasettelu:

    `localectl set-keymap fi`

5. Päivitä järjestelmä:

    `dnf update`

6. Asenna tarvittavat ohjelmat:

   `dnf install git`

Kääntäminen
1. Kloonaa repositorio

    ```
    git clone https://github.com/oturpe/montauk-chair-interface.git
    cd montauk-chair-interface
    ```

2. Asenna ohjelma

    `./install.sh`

Tämän jälkeen ohjelma käynnistyy seuraavan uudelleenkäynnistyksen yhteydessä.

Ohjelman voi myös käynnistää suoraan asennuksen jälkeen:

    `systemctl start montauk-chair-interface.service`
