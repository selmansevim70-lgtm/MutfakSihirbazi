# Mutfak Sihirbazı – Archicad 29 Eklentisi (C++)

Seçili duvarlara düz, L veya U mutfak dizer. Usta kurallarını kendisi uygular ve CSV kesim listesi çıkarır.
Archicad'in üst menüsünde **Mutfak** adıyla görünür.

| Menü | Ne yapar |
|---|---|
| **Mutfak Sihirbazı (seçili duvarlar)…** | 1–3 duvar seçip açarsınız. Modüller otomatik dizilir, isterseniz kodları düzenler, sonra **Oluştur**'a basarsınız. |
| **Kesim Listesi (CSV)…** | Seçili dolapların kesim listesini çıkarır. Seçim yoksa projedeki tüm mutfak dolaplarını kullanır. Liste parça, malzeme, boy × en, adet ve bant bilgisi içerir, sonunda malzeme başına m² özeti vardır. |
| **Mutfak Dolabını Nesne Aracına Yükle** | Dolap nesnesini Nesne aracına yükler, böylece tek tek elle yerleştirebilirsiniz. |

Dolap nesnesi (`MutfakDolabi.gsm`) eklentinin içine gömülüdür, ayrıca kütüphane yüklemeniz gerekmez.

---

## 1. Derleme (.apx dosyasını elde etmek)

### Yol A: GitHub ile, Visual Studio kurmadan (önerilen)
1. github.com'da **yeni ve boş bir depo** oluşturun (Private olabilir).
2. Bu klasörün **tüm içeriğini** depoya yükleyin. Gizli `.github` klasörü de yüklenmeli.
   (Tarayıcıdan "Add file > Upload files" ile sürükleyip bırakabilirsiniz.)
3. Deponun **Actions** sekmesini açın. "Derle (Archicad 29)" iş akışı kendiliğinden başlar (yaklaşık 10–15 dk).
4. İş bitince sayfanın altındaki **Artifacts** bölümünden indirin:
   - `MutfakSihirbazi-AC29-Windows` → içinde `MutfakSihirbazi.apx`
   - `MutfakSihirbazi-AC29-macOS` → içinde `MutfakSihirbazi.bundle`

Derleme hata verirse, Actions sayfasındaki kırmızı adımın kayıtlarını kopyalayıp bana gönderin.

### Yol B: Kendi bilgisayarınızda (Windows)
Gerekenler: Visual Studio 2022 ("Desktop development with C++" ve **v143** araç takımı), CMake 3.19+, Python 3.10+.
```
python Tools/BuildAddOn.py --configFile config.json --acVersion 29 --package
```
Sonuç `Build/Package/29/…zip` içinde olur. Archicad API Development Kit'i betik kendisi indirir.

---

## 2. Archicad'e yükleme
1. `.apx` dosyasını kalıcı bir klasöre koyun (ör. `Belgeler\Archicad Eklentileri`).
2. Archicad 29: **Seçenekler > Eklenti Yöneticisi** (Options > Add-On Manager).
3. **Gözat…** ile `.apx` dosyasını seçip ekleyin, işaretini açık bırakın.
4. Üst menüde **Mutfak** belirir.

> macOS'ta indirilen bundle karantinaya alınabilir. Terminal'de şu komutu çalıştırın:
> `xattr -dr com.apple.quarantine MutfakSihirbazi.bundle`

> Bu sürüm geliştirme kimliğiyle (MDID 1/1) derlenir, kendi kullanımınız için uygundur.
> Başkalarına dağıtacaksanız https://archicadapi.graphisoft.com adresinden ücretsiz bir MDID alın ve `RFIX/AddOnFix.grc` dosyasına yazın.

---

## 3. Kullanım
1. Planda mutfağın kurulacağı duvarları seçin:
   - **1 duvar** = düz mutfak
   - **2 duvar** = L mutfak
   - **3 duvar** = U mutfak
   Duvarlar köşelerde birleşik olmalı.
2. **Mutfak > Mutfak Sihirbazı…** komutunu açın.
3. Üst kısımda ölçüleri ayarlayın: alt/üst dolap yükseklik ve derinliği, tezgah, baza ve üst dolap alt kotu.
4. Her duvarın **Alt** satırı otomatik dolar. Kodları istediğiniz gibi değiştirebilirsiniz.
   **Üst** satırı boş bırakırsanız üst dolaplar alt sıraya hizalı olarak otomatik eklenir.
5. Önce **Kontrol et**, sonra **Oluştur** düğmesine basın. Tüm mutfak tek bir geri alma (Ctrl+Z) adımıdır.
6. Dolaplar yanlış yüze dizildiyse **"Dolapları duvarın diğer yüzüne yerleştir"** kutusunu işaretleyin.

### Modül kodları (genişlik cm + kod)
| Kod | Modül | Kod | Modül |
|---|---|---|---|
| `K` | Kapaklı | `BZ` | Buzdolabı (boy) |
| `C` | Çekmeceli (3 çekmece) | `B` | Boy dolap |
| `CK` | Üstte çekmece + kapak | `KK` | Kör köşe |
| `E` | Eviye dolabı (eviye + batarya) | `D` | Dolgu |
| `BM` | Bulaşık makinesi paneli | `A` | Açık raf |
| `F` | Ankastre fırın + ocak | `-` | Boşluk (dolap yok) |

Örnek: `60BZ 80E 60BM 75K 105KK`. Ondalık da yazabilirsiniz: `62,5K`.
Üst sırada yalnızca `K`, `KK`, `D`, `A` ve `-` kullanılır. Boy dolabın üstüne boşluk bırakmak için örneğin `60-` yazın.

### Usta kuralları (otomatik denetlenir)
- Tezgah altı **en az 87 cm**.
- Tezgah ile üst dolap arası **en az 58 cm**.
- Kapak **19 cm'den dar olamaz**. Daha dar modül dolguya çevrilir. Tek kapak en fazla 60 cm'dir, daha genişse otomatik iki kapak olur.
- Modül en fazla 120 cm olabilir.
- Dolaplar duvara sığmalı ve köşelerde **iç içe girmemeli**. Ayrıca plan üzerinde gerçek geometri ile çakışma denetimi yapılır.
- Bulaşık makinesi eviyenin yanında olmalı. Fırın 60 cm, bulaşık makinesi 45 veya 60 cm olmalı. Bunlara uymazsanız uyarı verilir.

### Köşe çözümü
Her köşede bir duvar köşenin sahibidir ve oraya **kör köşe** (`KK`) dolabı gelir. Diğer duvar, dolap derinliği kadar geriden başlar ve kulpların çarpmaması için önce **5 cm dolgu** (`5D`) konur.
Üst dolaplar da aynı mantıkla, kendi derinliklerine göre yerleşir.

---

## 4. Dolap nesnesini değiştirmek
Oluşturulan her dolap normal bir Archicad nesnesidir. Nesne Ayarları'ndan şunları değiştirebilirsiniz:
- tip ve ön düzen
- kapak, çekmece ve raf sayısı
- gövde ve kapak kalınlığı
- baza, tezgah ve kulp
- yüzeyler: gövde, kapak, tezgah, kulp ve cihaz

Yüzey değeri **0** ise nesne kendi varsayılan rengini kullanır: antrasit kapak, açık gövde, taş tezgah.
Kesim listesi, nesnelerin o anki parametrelerinden üretilir. Elle yaptığınız değişiklikler listeye yansır.

---

## 5. Klasör yapısı
```
Src/KitchenModel.*      Archicad'den bağımsız çekirdek: kodlar, otomatik dizme, kurallar, köşe, çakışma, kesim listesi
Src/KitchenAPI.*        Archicad API: duvar okuma, nesne oluşturma, CSV kaydetme
Src/KitchenDialog.*     Diyalog
Src/AddOnMain.cpp       Menü ve eklenti giriş noktaları
RINT/AddOn.grc          Menü metinleri ve diyalog düzeni
RINT/ACLib/Src/MutfakDolabi/   Gömülü GDL dolap nesnesi (HSF kaynak)
Tests/ModelTest.cpp     Çekirdek testleri (Archicad olmadan: g++ -std=c++20 -I../Src ModelTest.cpp ../Src/KitchenModel.cpp)
Tools/                  Graphisoft'un resmi derleme araçları (MIT lisansı)
```

## 6. Bilinen sınırlamalar (sonraki sürüm için)
- Eğri ve trapez duvarlar desteklenmiyor.
- Pencere altına üst dolap koymama, kiriş ve kolon algılama henüz yok.
- Kesim listesinde çekmece kutuları ve levha yerleşimi (nesting) yok.
- CNC çıktıları (DXF, woodWOP) ve teklif/fiyat hesabı yok.
