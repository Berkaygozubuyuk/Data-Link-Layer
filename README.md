# Data-Link-Layer
Bu projede, veri iletimi sırasında oluşabilecek hataları simüle eden ve CRC kontrolüyle güvenliği
sağlayan bir Qt GUI uygulaması geliştirilmiştir. Uygulama, seçilen dat uzantılı dosyayı okuyarak
verileri bit dizisine dönüştürmekte ve 100 bitlik framelere ayırarak her birine 2 bitlik header, 4
byte sıra numarası ve CRC eklemektedir. Gönderilen veri bozulabilir, kaybolabilir veya başarılı
iletilebilir. Tüm süreç listelerde kullanıcıya görsel olarak sunulmaktadır.
