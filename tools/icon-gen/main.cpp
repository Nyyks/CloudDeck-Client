// Generates every raster icon from tools/icon-gen/*.svg.
// Usage (repo root): icon-gen <repo-root>
//   QT_QPA_PLATFORM=offscreen ./icon-gen /path/to/CloudDeck
#include <QBuffer>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QSvgRenderer>
#include <QtEndian>
#include <cstdio>

static QImage render(const QString& svg, int size)
{
    QSvgRenderer r(svg);
    if (!r.isValid()) { std::fprintf(stderr, "invalid svg %s\n", qPrintable(svg)); std::exit(1); }
    QImage img(size, size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    r.render(&p, QRectF(0, 0, size, size));
    return img;
}

static void savePng(const QImage& img, const QString& path)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    if (!img.save(path, "PNG")) { std::fprintf(stderr, "write failed %s\n", qPrintable(path)); std::exit(1); }
    std::printf("wrote %s\n", qPrintable(path));
}

// ICO with PNG-compressed entries (supported since Windows Vista).
static void saveIco(const QString& svg, const QList<int>& sizes, const QString& path)
{
    QList<QByteArray> pngs;
    for (int s : sizes) {
        QByteArray b; QBuffer buf(&b); buf.open(QIODevice::WriteOnly);
        render(svg, s).save(&buf, "PNG");
        pngs << b;
    }
    QByteArray out;
    auto u16 = [&](quint16 v) { char c[2]; qToLittleEndian(v, c); out.append(c, 2); };
    auto u32 = [&](quint32 v) { char c[4]; qToLittleEndian(v, c); out.append(c, 4); };
    u16(0); u16(1); u16(quint16(sizes.size()));
    quint32 offset = 6 + 16 * sizes.size();
    for (int i = 0; i < sizes.size(); i++) {
        out.append(char(sizes[i] >= 256 ? 0 : sizes[i]));
        out.append(char(sizes[i] >= 256 ? 0 : sizes[i]));
        out.append(char(0)); out.append(char(0));
        u16(1); u16(32);
        u32(quint32(pngs[i].size())); u32(offset);
        offset += pngs[i].size();
    }
    for (const QByteArray& p : pngs) out.append(p);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(out) != out.size()) { std::fprintf(stderr, "write failed %s\n", qPrintable(path)); std::exit(1); }
    std::printf("wrote %s\n", qPrintable(path));
}

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    if (argc != 2) { std::fprintf(stderr, "usage: icon-gen <repo-root>\n"); return 2; }
    const QString root = QString::fromLocal8Bit(argv[1]);
    const QString icon = root + "/tools/icon-gen/icon.svg";
    const QString fg = root + "/tools/icon-gen/foreground.svg";

    // Desktop: Windows exe/installer icon, WiX banner icon, in-app logo source
    saveIco(icon, {16, 24, 32, 48, 64, 128, 256}, root + "/client-desktop/app/moonlight.ico");
    savePng(render(icon, 64), root + "/client-desktop/app/moonlight_wix.png");
    for (const char* dst : {"/client-desktop/app/res/moonlight.svg",
                            "/client-desktop/app/clouddeck/deploy/linux/io.github.nyyks.CloudDeckClient.svg"}) {
        QFile::remove(root + dst);
        if (!QFile::copy(icon, root + dst)) { std::fprintf(stderr, "copy failed %s\n", dst); return 1; }
        std::printf("copied icon.svg -> %s\n", dst);
    }

    // Android: legacy launcher icons + adaptive foreground per density
    const QString res = root + "/client-android/app/src/main/res";
    const struct { const char* dpi; int legacy; int adaptive; } d[] = {
        {"mdpi", 48, 108}, {"hdpi", 72, 162}, {"xhdpi", 96, 216}, {"xxhdpi", 144, 324}, {"xxxhdpi", 192, 432},
    };
    for (const auto& e : d) {
        savePng(render(icon, e.legacy), res + "/mipmap-" + e.dpi + "/ic_launcher.png");
        savePng(render(fg, e.adaptive), res + "/mipmap-" + e.dpi + "/ic_launcher_foreground.png");
    }
    return 0;
}
