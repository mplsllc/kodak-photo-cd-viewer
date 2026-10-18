// SPDX-License-Identifier: GPL-3.0-or-later
#include "MainWindow.h"
#include "PhotoCdExporter.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QStackedWidget>
#include <QStatusBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QPainter>
#include <QFrame>
#include <QProgressDialog>
#include <QStandardPaths>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QFont>
#include <algorithm>
#include <functional>
#include <QtConcurrent/QtConcurrentRun>

namespace {
QString resolutionName(PcdDecoder::Resolution resolution)
{
    switch (resolution) {
    case PcdDecoder::Base16: return QStringLiteral("16Base preview");
    case PcdDecoder::Base4: return QStringLiteral("Base/4 preview");
    case PcdDecoder::Base: return QStringLiteral("Base");
    case PcdDecoder::FourBase: return QStringLiteral("4Base");
    case PcdDecoder::SixteenBase: return QStringLiteral("16Base");
    }
    return QStringLiteral("Photo CD");
}
}

class PhotoCanvas final : public QGraphicsView {
public:
    explicit PhotoCanvas(QWidget *parent = nullptr)
        : QGraphicsView(parent), scene_(new QGraphicsScene(this))
    {
        setScene(scene_);
        setBackgroundBrush(QColor(QStringLiteral("#111b20")));
        setFrameShape(QFrame::NoFrame);
        setDragMode(QGraphicsView::ScrollHandDrag);
        setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
        setResizeAnchor(QGraphicsView::AnchorViewCenter);
        setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    }

    void showImage(const QImage &image)
    {
        scene_->clear();
        item_ = scene_->addPixmap(QPixmap::fromImage(image));
        item_->setTransformOriginPoint(item_->boundingRect().center());
        scene_->setSceneRect(item_->boundingRect());
        resetTransform();
        fitInView(item_, Qt::KeepAspectRatio);
        fitted_ = true;
    }

    void zoomBy(qreal factor)
    {
        if (!item_)
            return;
        scale(factor, factor);
        fitted_ = false;
    }

    void rotateClockwise()
    {
        if (!item_)
            return;
        item_->setRotation(item_->rotation() + 90.0);
        scene_->setSceneRect(item_->sceneBoundingRect());
        resetTransform();
        fitInView(item_, Qt::KeepAspectRatio);
        fitted_ = true;
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QGraphicsView::resizeEvent(event);
        if (item_ && fitted_)
            fitInView(item_, Qt::KeepAspectRatio);
    }

private:
    QGraphicsScene *scene_ = nullptr;
    QGraphicsPixmapItem *item_ = nullptr;
    bool fitted_ = true;
};

namespace {
QPushButton *makeButton(const QString &text, const QString &name, QWidget *parent)
{
    auto *button = new QPushButton(text, parent);
    button->setObjectName(name);
    button->setMinimumHeight(38);
    button->setCursor(Qt::PointingHandCursor);
    return button;
}
}

MainWindow::MainWindow(const QString &initialPath)
{
    setWindowTitle(QStringLiteral("Photo CD Player"));
    resize(1160, 780);
    applyStyle();
    createPages();
    createMenus();
    slideshowTimer_.setInterval(QSettings().value(QStringLiteral("slideshow/seconds"), 5).toInt() * 1000);
    connect(&slideshowTimer_, &QTimer::timeout, this, [this] { advanceSlideshow(); });

    if (!initialPath.isEmpty())
        openDiscPath(initialPath);
    else
        discoverDisc();
    showPage(Page::Welcome);
}

void MainWindow::applyStyle()
{
    qApp->setStyle(QStringLiteral("Fusion"));
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(QStringLiteral("#e9e3d3")));
    palette.setColor(QPalette::WindowText, QColor(QStringLiteral("#1b252a")));
    palette.setColor(QPalette::Base, QColor(QStringLiteral("#f7f3e9")));
    palette.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#ded8c8")));
    palette.setColor(QPalette::Button, QColor(QStringLiteral("#e4ddca")));
    palette.setColor(QPalette::ButtonText, QColor(QStringLiteral("#1b252a")));
    palette.setColor(QPalette::Highlight, QColor(QStringLiteral("#b84c2c")));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    qApp->setPalette(palette);
    qApp->setFont(QFont(QStringLiteral("DejaVu Sans"), 10));

    setStyleSheet(QStringLiteral(
        "QMainWindow { background: #e9e3d3; }"
        "QPushButton, QToolButton { background: #eee8d9; border: 1px solid #53636a;"
        " padding: 7px 12px; color: #1b252a; }"
        "QPushButton:hover, QToolButton:hover { border-color: #b84c2c; background: #f8f0dd; }"
        "QPushButton:disabled { color: #86847d; border-color: #aaa69a; }"
        "QMenuBar { background: #e1dac8; border-bottom: 1px solid #a9a08b; }"
        "QMenuBar::item:selected, QMenu::item:selected { background: #b84c2c; color: white; }"
        "QToolButton { border-radius: 0; }"
        "QScrollArea { border: 1px solid #9c988b; background: #ddd7c7; }"
    ));
}

void MainWindow::createPages()
{
    auto *central = new QWidget(this);
    auto *outer = new QVBoxLayout(central);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto *header = new QWidget(central);
    header->setStyleSheet(QStringLiteral("background:#203d50; color:#f5ead2;"));
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(20, 10, 20, 10);
    auto *brand = new QLabel(QStringLiteral("PHOTO CD"), header);
    brand->setStyleSheet(QStringLiteral("font: bold 18pt 'DejaVu Sans Mono'; letter-spacing: 2px;"));
    albumLabel_ = new QLabel(QStringLiteral("No disc loaded"), header);
    albumLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    headerLayout->addWidget(brand);
    headerLayout->addStretch(1);
    headerLayout->addWidget(albumLabel_);
    outer->addWidget(header);

    auto *rule = new QFrame(central);
    rule->setFixedHeight(5);
    rule->setStyleSheet(QStringLiteral("background:#b84c2c; border:0;"));
    outer->addWidget(rule);

    pages_ = new QStackedWidget(central);
    outer->addWidget(pages_, 1);
    setCentralWidget(central);

    welcomePage_ = new QWidget(pages_);
    auto *welcomeLayout = new QHBoxLayout(welcomePage_);
    welcomeLayout->setContentsMargins(26, 22, 26, 22);
    welcomeLayout->setSpacing(24);

    auto *artColumn = new QVBoxLayout;
    startupLabel_ = new QLabel(QStringLiteral("Photo CD startup artwork"), welcomePage_);
    startupLabel_->setAlignment(Qt::AlignCenter);
    startupLabel_->setMinimumSize(380, 280);
    startupLabel_->setScaledContents(true);
    startupLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    startupLabel_->setStyleSheet(QStringLiteral("background:#17242a; color:#d8d1bd; border:1px solid #6f716c;"));
    artColumn->addWidget(startupLabel_, 1);
    welcomeStatus_ = new QLabel(QStringLiteral("Insert and mount a Photo CD, then choose Open Disc."), welcomePage_);
    welcomeStatus_->setWordWrap(true);
    welcomeStatus_->setStyleSheet(QStringLiteral("color:#454a48; padding:5px;"));
    artColumn->addWidget(welcomeStatus_);
    welcomeLayout->addLayout(artColumn, 3);

    auto *menuBox = new QGroupBox(QStringLiteral("Opening screen"), welcomePage_);
    auto *menuLayout = new QVBoxLayout(menuBox);
    auto *menuIntro = new QLabel(QStringLiteral("Choose an operation for this Photo CD."), menuBox);
    menuIntro->setWordWrap(true);
    menuLayout->addWidget(menuIntro);

    auto *exportAll = makeButton(QStringLiteral("Export all photographs…"), QStringLiteral("requiresAlbum"), menuBox);
    exportAll->setProperty("actionName", QStringLiteral("exportAllPhotographs"));
    exportAll->setToolTip(QStringLiteral("Save every photograph as a high-resolution JPEG in a folder you choose."));
    connect(exportAll, &QPushButton::clicked, this, [this] { exportAllPhotos(); });
    menuLayout->addWidget(exportAll);

    auto *viewPhotos = makeButton(QStringLiteral("View photographs"), QStringLiteral("requiresAlbum"), menuBox);
    viewPhotos->setEnabled(false);
    connect(viewPhotos, &QPushButton::clicked, this, [this] {
        if (album_ && !album_->photos.isEmpty()) {
            showPage(Page::Viewer);
            loadPhoto(currentIndex_);
        }
    });
    menuLayout->addWidget(viewPhotos);

    auto *viewIndex = makeButton(QStringLiteral("View index of photographs"), QStringLiteral("requiresAlbum"), menuBox);
    viewIndex->setEnabled(false);
    connect(viewIndex, &QPushButton::clicked, this, [this] { showIndex(false); });
    menuLayout->addWidget(viewIndex);

    auto *selectFavorites = makeButton(QStringLiteral("Select favourites"), QStringLiteral("requiresAlbum"), menuBox);
    selectFavorites->setEnabled(false);
    connect(selectFavorites, &QPushButton::clicked, this, [this] { showIndex(true); });
    menuLayout->addWidget(selectFavorites);

    auto *playAll = makeButton(QStringLiteral("Play slide show"), QStringLiteral("requiresAlbum"), menuBox);
    playAll->setEnabled(false);
    connect(playAll, &QPushButton::clicked, this, [this] { startSlideshow(false); });
    menuLayout->addWidget(playAll);

    auto *playFavorites = makeButton(QStringLiteral("Play favourites"), QStringLiteral("requiresAlbum"), menuBox);
    playFavorites->setEnabled(false);
    connect(playFavorites, &QPushButton::clicked, this, [this] { startSlideshow(true); });
    menuLayout->addWidget(playFavorites);

    auto *open = makeButton(QStringLiteral("Open Photo CD…"), QStringLiteral("openDisc"), menuBox);
    connect(open, &QPushButton::clicked, this, [this] { openDiscDialog(); });
    menuLayout->addWidget(open);

    auto *refresh = makeButton(QStringLiteral("Find mounted discs"), QStringLiteral("refreshDiscs"), menuBox);
    connect(refresh, &QPushButton::clicked, this, [this] { discoverDisc(); });
    menuLayout->addWidget(refresh);
    menuLayout->addStretch(1);
    welcomeLayout->addWidget(menuBox, 2);
    pages_->addWidget(welcomePage_);

    viewerPage_ = new QWidget(pages_);
    auto *viewerLayout = new QVBoxLayout(viewerPage_);
    viewerLayout->setContentsMargins(18, 14, 18, 14);
    viewerLayout->setSpacing(8);
    auto *titleRow = new QHBoxLayout;
    photoTitle_ = new QLabel(QStringLiteral("Photograph"), viewerPage_);
    photoTitle_->setStyleSheet(QStringLiteral("font: bold 14pt 'DejaVu Sans Mono'; color:#203d50;"));
    photoCount_ = new QLabel(viewerPage_);
    titleRow->addWidget(photoTitle_);
    titleRow->addStretch(1);
    titleRow->addWidget(photoCount_);
    viewerLayout->addLayout(titleRow);
    viewerMessage_ = new QLabel(QStringLiteral("Loading photograph…"), viewerPage_);
    viewerMessage_->setAlignment(Qt::AlignCenter);
    viewerMessage_->setStyleSheet(QStringLiteral("color:#555;"));
    viewerLayout->addWidget(viewerMessage_);
    photoView_ = new PhotoCanvas(viewerPage_);
    viewerLayout->addWidget(photoView_, 1);

    auto *controls = new QHBoxLayout;
    auto addControl = [controls, viewerPage_ = viewerPage_](const QString &text, const std::function<void()> &action) {
        auto *button = new QPushButton(text, viewerPage_);
        button->setMinimumHeight(38);
        QObject::connect(button, &QPushButton::clicked, viewerPage_, action);
        controls->addWidget(button);
        return button;
    };
    addControl(QStringLiteral("◀ Previous"), [this] { loadPhoto((currentIndex_ + album_->photos.size() - 1) % album_->photos.size()); });
    addControl(QStringLiteral("Next ▶"), [this] { loadPhoto((currentIndex_ + 1) % album_->photos.size()); });
    addControl(QStringLiteral("Index"), [this] { showIndex(false); });
    addControl(QStringLiteral("− Zoom"), [this] { photoView_->zoomBy(1.0 / 1.2); });
    addControl(QStringLiteral("+ Zoom"), [this] { photoView_->zoomBy(1.2); });
    addControl(QStringLiteral("Rotate 90°"), [this] { photoView_->rotateClockwise(); });
    favoriteButton_ = new QToolButton(viewerPage_);
    favoriteButton_->setText(QStringLiteral("☆ Favourite"));
    favoriteButton_->setMinimumHeight(38);
    connect(favoriteButton_, &QToolButton::clicked, this, [this] { toggleFavorite(); });
    controls->addWidget(favoriteButton_);
    addControl(QStringLiteral("▶ Slide show"), [this] { startSlideshow(false); });
    addControl(QStringLiteral("Welcome"), [this] { pauseSlideshow(); showWelcome(); });
    viewerLayout->addLayout(controls);
    pages_->addWidget(viewerPage_);

    indexPage_ = new QWidget(pages_);
    auto *indexLayout = new QVBoxLayout(indexPage_);
    indexLayout->setContentsMargins(18, 14, 18, 14);
    auto *indexHeader = new QHBoxLayout;
    indexTitle_ = new QLabel(QStringLiteral("Index of photographs"), indexPage_);
    indexTitle_->setStyleSheet(QStringLiteral("font: bold 14pt 'DejaVu Sans Mono'; color:#203d50;"));
    indexHeader->addWidget(indexTitle_);
    indexHeader->addStretch(1);
    auto *allButton = new QPushButton(QStringLiteral("All photographs"), indexPage_);
    connect(allButton, &QPushButton::clicked, this, [this] { showIndex(false); });
    indexHeader->addWidget(allButton);
    auto *favButton = new QPushButton(QStringLiteral("Favourites"), indexPage_);
    connect(favButton, &QPushButton::clicked, this, [this] { showIndex(true); });
    indexHeader->addWidget(favButton);
    auto *backButton = new QPushButton(QStringLiteral("Welcome"), indexPage_);
    connect(backButton, &QPushButton::clicked, this, [this] { showWelcome(); });
    indexHeader->addWidget(backButton);
    indexLayout->addLayout(indexHeader);

    auto *scroll = new QScrollArea(indexPage_);
    scroll->setWidgetResizable(true);
    auto *tiles = new QWidget(scroll);
    thumbnailGrid_ = new QGridLayout(tiles);
    thumbnailGrid_->setContentsMargins(14, 14, 14, 14);
    thumbnailGrid_->setSpacing(12);
    scroll->setWidget(tiles);
    indexLayout->addWidget(scroll, 1);
    pages_->addWidget(indexPage_);

    statusBar()->showMessage(QStringLiteral("Ready"));
}

void MainWindow::createMenus()
{
    auto *fileMenu = menuBar()->addMenu(QStringLiteral("File"));
    auto *openAction = fileMenu->addAction(QStringLiteral("Open Photo CD…"));
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, [this] { openDiscDialog(); });
    exportAllAction_ = fileMenu->addAction(QStringLiteral("Export all photographs…"));
    exportAllAction_->setObjectName(QStringLiteral("exportAllPhotographsAction"));
    exportAllAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E));
    exportAllAction_->setEnabled(false);
    connect(exportAllAction_, &QAction::triggered, this, [this] { exportAllPhotos(); });
    auto *refreshAction = fileMenu->addAction(QStringLiteral("Find mounted discs"));
    connect(refreshAction, &QAction::triggered, this, [this] { discoverDisc(); });
    fileMenu->addSeparator();
    auto *quitAction = fileMenu->addAction(QStringLiteral("Quit"));
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &QWidget::close);

    auto *viewMenu = menuBar()->addMenu(QStringLiteral("View"));
    auto *welcomeAction = viewMenu->addAction(QStringLiteral("Opening screen"));
    welcomeAction->setShortcut(Qt::Key_Escape);
    connect(welcomeAction, &QAction::triggered, this, [this] { pauseSlideshow(); showWelcome(); });
    auto *indexAction = viewMenu->addAction(QStringLiteral("Index"));
    indexAction->setShortcut(Qt::Key_I);
    connect(indexAction, &QAction::triggered, this, [this] { showIndex(false); });
    auto *favoritesAction = viewMenu->addAction(QStringLiteral("Favourites index"));
    connect(favoritesAction, &QAction::triggered, this, [this] { showIndex(true); });

    auto *photoMenu = menuBar()->addMenu(QStringLiteral("Photograph"));
    auto *previousAction = photoMenu->addAction(QStringLiteral("Previous photograph"));
    previousAction->setShortcut(Qt::Key_Left);
    connect(previousAction, &QAction::triggered, this, [this] {
        if (album_ && !album_->photos.isEmpty()) loadPhoto((currentIndex_ + album_->photos.size() - 1) % album_->photos.size());
    });
    auto *nextAction = photoMenu->addAction(QStringLiteral("Next photograph"));
    nextAction->setShortcut(Qt::Key_Right);
    connect(nextAction, &QAction::triggered, this, [this] {
        if (album_ && !album_->photos.isEmpty()) loadPhoto((currentIndex_ + 1) % album_->photos.size());
    });
    photoMenu->addSeparator();
    auto *zoomIn = photoMenu->addAction(QStringLiteral("Zoom in"));
    zoomIn->setShortcut(QKeySequence::ZoomIn);
    connect(zoomIn, &QAction::triggered, this, [this] { photoView_->zoomBy(1.2); });
    auto *zoomOut = photoMenu->addAction(QStringLiteral("Zoom out"));
    zoomOut->setShortcut(QKeySequence::ZoomOut);
    connect(zoomOut, &QAction::triggered, this, [this] { photoView_->zoomBy(1.0 / 1.2); });
    auto *rotate = photoMenu->addAction(QStringLiteral("Rotate clockwise"));
    rotate->setShortcut(Qt::Key_R);
    connect(rotate, &QAction::triggered, this, [this] { photoView_->rotateClockwise(); });
    auto *favorite = photoMenu->addAction(QStringLiteral("Toggle favourite"));
    favorite->setShortcut(Qt::Key_F);
    connect(favorite, &QAction::triggered, this, [this] { toggleFavorite(); });

    auto *resolutionMenu = photoMenu->addMenu(QStringLiteral("Resolution"));
    auto *resolutionGroup = new QActionGroup(this);
    resolutionGroup->setExclusive(true);
    const QList<QPair<QString, PcdDecoder::Resolution>> resolutions = {
        {QStringLiteral("Base16 preview"), PcdDecoder::Base16},
        {QStringLiteral("Base4"), PcdDecoder::Base4},
        {QStringLiteral("Base"), PcdDecoder::Base},
        {QStringLiteral("4Base"), PcdDecoder::FourBase},
        {QStringLiteral("16Base"), PcdDecoder::SixteenBase}
    };
    for (const auto &entry : resolutions) {
        auto *action = resolutionMenu->addAction(entry.first);
        action->setCheckable(true);
        action->setChecked(entry.second == resolution_);
        resolutionGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, resolution = entry.second] { setResolution(resolution); });
    }

    auto *playMenu = menuBar()->addMenu(QStringLiteral("Playback"));
    auto *playAction = playMenu->addAction(QStringLiteral("Play all photographs"));
    playAction->setShortcut(Qt::Key_Space);
    connect(playAction, &QAction::triggered, this, [this] { startSlideshow(false); });
    auto *playFavoritesAction = playMenu->addAction(QStringLiteral("Play favourites"));
    connect(playFavoritesAction, &QAction::triggered, this, [this] { startSlideshow(true); });
    auto *pauseAction = playMenu->addAction(QStringLiteral("Pause slide show"));
    pauseAction->setShortcut(Qt::SHIFT | Qt::Key_Space);
    connect(pauseAction, &QAction::triggered, this, [this] { pauseSlideshow(); });
    auto *speedMenu = playMenu->addMenu(QStringLiteral("Seconds per photograph"));
    auto *speedGroup = new QActionGroup(this);
    speedGroup->setExclusive(true);
    for (const int seconds : {3, 5, 10}) {
        auto *action = speedMenu->addAction(QStringLiteral("%1 seconds").arg(seconds));
        action->setCheckable(true);
        action->setChecked(QSettings().value(QStringLiteral("slideshow/seconds"), 5).toInt() == seconds);
        speedGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, seconds] {
            QSettings().setValue(QStringLiteral("slideshow/seconds"), seconds);
            slideshowTimer_.setInterval(seconds * 1000);
        });
    }
}

void MainWindow::discoverDisc()
{
    QString error;
    auto album = PhotoCdAlbum::discoverMounted(&error);
    if (!album) {
        if (!album_)
            welcomeStatus_->setText(QStringLiteral("No mounted Photo CD found. Insert and mount a disc, or use Open Photo CD."));
        statusBar()->showMessage(error, 6000);
        return;
    }
    setAlbum(std::move(*album));
    statusBar()->showMessage(QStringLiteral("Mounted Photo CD found."), 4000);
}

void MainWindow::openDiscDialog()
{
    const QString path = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Open a mounted Photo CD"), QStringLiteral("/media"),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (!path.isEmpty())
        openDiscPath(path);
}

void MainWindow::exportAllPhotos()
{
    if (!album_ || album_->photos.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("No Photo CD"),
                                 QStringLiteral("Open a Photo CD before exporting photographs."));
        return;
    }

    const QString picturesDirectory = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    const QString lastDirectory = QSettings().value(QStringLiteral("export/lastDirectory")).toString();
    const QString initialDirectory = QFileInfo(lastDirectory).isDir() ? lastDirectory
        : (!picturesDirectory.isEmpty() && QFileInfo(picturesDirectory).isDir()
               ? picturesDirectory : QDir::homePath());
    const QString destination = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Choose a folder for all exported photographs"), initialDirectory,
        QFileDialog::ShowDirsOnly);
    if (destination.isEmpty())
        return;
    QSettings().setValue(QStringLiteral("export/lastDirectory"), destination);

    pauseSlideshow();
    QProgressDialog progress(QStringLiteral("Preparing high-resolution JPEG export…"),
                             QStringLiteral("Cancel export"), 0, album_->photos.size(), this);
    progress.setWindowTitle(QStringLiteral("Export Photo CD"));
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.setAutoClose(false);
    progress.setAutoReset(false);
    progress.show();
    QApplication::processEvents();

    const PhotoCdExportResult result = PhotoCdExporter::exportAll(
        *album_, destination,
        [&progress](int completed, int total, const QString &name) {
            progress.setLabelText(QStringLiteral("Exporting %1 (%2 of %3)…")
                                      .arg(name).arg(completed + 1).arg(total));
            progress.setValue(completed);
            QApplication::processEvents();
        },
        [&progress] { return progress.wasCanceled(); });
    if (!result.canceled)
        progress.setValue(album_->photos.size());
    progress.hide();

    QString message = QStringLiteral("Exported %1 photograph(s).\nSkipped %2 existing file(s).\nFailed %3 photograph(s).")
                          .arg(result.exported).arg(result.skipped).arg(result.failed);
    if (result.canceled)
        message += QStringLiteral("\n\nExport canceled. Files already completed are kept.");
    if (!result.errors.isEmpty()) {
        const QStringList details = result.errors.mid(0, 8);
        message += QStringLiteral("\n\n") + details.join(QLatin1Char('\n'));
        if (result.errors.size() > details.size())
            message += QStringLiteral("\n…and %1 more error(s).")
                           .arg(result.errors.size() - details.size());
    }
    if (result.failed > 0)
        QMessageBox::warning(this, QStringLiteral("Export finished with errors"), message);
    else
        QMessageBox::information(this, QStringLiteral("Export complete"), message);
}

void MainWindow::openDiscPath(const QString &path)
{
    QString error;
    auto album = PhotoCdAlbum::open(path, &error);
    if (!album) {
        QMessageBox::warning(this, QStringLiteral("Photo CD not found"), error);
        return;
    }
    setAlbum(std::move(*album));
}

void MainWindow::setAlbum(PhotoCdAlbum album)
{
    pauseSlideshow();
    album_ = std::move(album);
    currentIndex_ = 0;
    thumbnailCache_.clear();
    favorites_.clear();
    const QStringList saved = QSettings().value(QStringLiteral("albums/%1/favorites").arg(album_->identity)).toStringList();
    for (const QString &number : saved) {
        bool ok = false;
        const int value = number.toInt(&ok);
        if (ok)
            favorites_.insert(value);
    }
    albumLabel_->setText(QStringLiteral("%1  ·  %2 photographs").arg(album_->volumeName).arg(album_->photos.size()));
    welcomeStatus_->setText(QStringLiteral("%1 photographs · mounted album %2").arg(album_->photos.size()).arg(album_->volumeName));
    for (QPushButton *button : welcomePage_->findChildren<QPushButton *>())
        if (button->objectName() == QStringLiteral("requiresAlbum")) button->setEnabled(true);
    if (exportAllAction_)
        exportAllAction_->setEnabled(true);
    loadStartupArtwork();
    updatePhotoLabels();
    showWelcome();
    statusBar()->showMessage(QStringLiteral("%1 photographs ready. The disc is read directly from its mounted folder.").arg(album_->photos.size()), 6000);
}

void MainWindow::showPage(Page page)
{
    page_ = page;
    if (page == Page::Welcome) pages_->setCurrentWidget(welcomePage_);
    else if (page == Page::Viewer) pages_->setCurrentWidget(viewerPage_);
    else pages_->setCurrentWidget(indexPage_);
}

void MainWindow::showWelcome()
{
    pauseSlideshow();
    showPage(Page::Welcome);
}

void MainWindow::loadStartupArtwork()
{
    startupLabel_->clear();
    if (!album_ || album_->startupPath.isEmpty()) {
        startupLabel_->setText(QStringLiteral("No STARTUP.PCD artwork"));
        return;
    }
    const DecodeResult result = PcdDecoder::decode(album_->startupPath, PcdDecoder::Base4);
    if (!result.ok()) {
        startupLabel_->setText(QStringLiteral("STARTUP.PCD could not be decoded"));
        statusBar()->showMessage(QStringLiteral("Startup artwork: %1").arg(result.error), 7000);
        return;
    }
    startupLabel_->setPixmap(QPixmap::fromImage(result.image));
    startupLabel_->setToolTip(QStringLiteral("Decoded directly from STARTUP.PCD"));
}

void MainWindow::loadPhoto(int index)
{
    if (!album_ || album_->photos.isEmpty())
        return;
    if (index < 0 || index >= album_->photos.size())
        return;
    currentIndex_ = index;
    updatePhotoLabels();
    showPage(Page::Viewer);
    viewerMessage_->setText(QStringLiteral("Reading %1 at %2…")
                            .arg(album_->photos.at(index).name, resolutionName(resolution_)));
    const QString path = album_->photos.at(index).path;
    const auto resolution = resolution_;
    const quint64 request = ++imageRequest_;
    auto *watcher = new QFutureWatcher<DecodeResult>(this);
    connect(watcher, &QFutureWatcher<DecodeResult>::finished, this, [this, watcher, request] {
        const DecodeResult result = watcher->result();
        watcher->deleteLater();
        if (request != imageRequest_)
            return;
        if (!result.ok()) {
            viewerMessage_->setText(result.error);
            statusBar()->showMessage(result.error, 10000);
            return;
        }
        viewerMessage_->clear();
        photoView_->showImage(result.image);
        statusBar()->showMessage(QStringLiteral("%1 × %2 · %3")
                                     .arg(result.image.width()).arg(result.image.height())
                                     .arg(resolutionName(resolution_)), 4000);
    });
    watcher->setFuture(QtConcurrent::run([path, resolution] { return PcdDecoder::decode(path, resolution); }));
}

void MainWindow::updatePhotoLabels()
{
    if (!album_ || album_->photos.isEmpty()) {
        photoTitle_->setText(QStringLiteral("No photograph selected"));
        photoCount_->clear();
        if (favoriteButton_) favoriteButton_->setText(QStringLiteral("☆ Favourite"));
        return;
    }
    const PhotoEntry &photo = album_->photos.at(currentIndex_);
    photoTitle_->setText(QStringLiteral("Photograph %1").arg(photo.number, 4, 10, QLatin1Char('0')));
    photoCount_->setText(QStringLiteral("%1 of %2").arg(currentIndex_ + 1).arg(album_->photos.size()));
    if (favoriteButton_)
        favoriteButton_->setText(favorites_.contains(photo.number) ? QStringLiteral("★ Favourite") : QStringLiteral("☆ Favourite"));
}

void MainWindow::showIndex(bool favoritesOnly)
{
    if (!album_)
        return;
    pauseSlideshow();
    favoritesIndex_ = favoritesOnly;
    clearIndex();
    indexTitle_->setText(favoritesOnly ? QStringLiteral("Favourite picture selection") : QStringLiteral("Index of photographs"));
    int shown = 0;
    for (int index = 0; index < album_->photos.size(); ++index) {
        const PhotoEntry &photo = album_->photos.at(index);
        if (favoritesOnly && !favorites_.contains(photo.number))
            continue;

        auto *tile = new QToolButton(indexPage_);
        tile->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        tile->setIconSize(QSize(128, 96));
        tile->setMinimumSize(154, 132);
        tile->setText(favorites_.contains(photo.number)
                          ? QStringLiteral("★  %1").arg(photo.number, 4, 10, QLatin1Char('0'))
                          : QStringLiteral("%1").arg(photo.number, 4, 10, QLatin1Char('0')));
        tile->setToolTip(photo.name);
        tile->setCursor(Qt::PointingHandCursor);
        auto image = thumbnailCache_.constFind(photo.number);
        if (image == thumbnailCache_.cend()) {
            const DecodeResult decoded = PcdDecoder::decode(photo.path, PcdDecoder::Base16);
            if (decoded.ok()) {
                thumbnailCache_.insert(photo.number, decoded.image);
                tile->setIcon(QPixmap::fromImage(decoded.image));
            } else {
                tile->setText(QStringLiteral("%1\n(no preview)").arg(photo.number, 4, 10, QLatin1Char('0')));
                tile->setToolTip(decoded.error);
            }
        } else {
            tile->setIcon(QPixmap::fromImage(image.value()));
        }
        connect(tile, &QToolButton::clicked, this, [this, index] { loadPhoto(index); });
        thumbnailGrid_->addWidget(tile, shown / 5, shown % 5);
        ++shown;
        QApplication::processEvents();
    }
    if (shown == 0) {
        auto *empty = new QLabel(favoritesOnly ? QStringLiteral("No favourites have been selected yet.")
                                               : QStringLiteral("No photographs are available."), indexPage_);
        empty->setAlignment(Qt::AlignCenter);
        thumbnailGrid_->addWidget(empty, 0, 0);
    }
    showPage(Page::Index);
    statusBar()->showMessage(QStringLiteral("Showing %1 of %2 photographs.").arg(shown).arg(album_->photos.size()), 4000);
}

void MainWindow::clearIndex()
{
    while (QLayoutItem *item = thumbnailGrid_->takeAt(0)) {
        if (QWidget *widget = item->widget())
            delete widget;
        delete item;
    }
}

void MainWindow::toggleFavorite()
{
    if (!album_ || album_->photos.isEmpty())
        return;
    const int number = album_->photos.at(currentIndex_).number;
    if (favorites_.contains(number))
        favorites_.remove(number);
    else
        favorites_.insert(number);
    saveFavorites();
    updatePhotoLabels();
    if (page_ == Page::Index)
        showIndex(favoritesIndex_);
}

void MainWindow::saveFavorites()
{
    if (!album_)
        return;
    QStringList values;
    QList<int> sorted = favorites_.values();
    std::sort(sorted.begin(), sorted.end());
    for (const int number : sorted)
        values.push_back(QString::number(number));
    QSettings().setValue(QStringLiteral("albums/%1/favorites").arg(album_->identity), values);
    welcomeStatus_->setText(QStringLiteral("%1 photographs · %2 favourites · mounted album %3")
                                .arg(album_->photos.size()).arg(favorites_.size()).arg(album_->volumeName));
}

void MainWindow::startSlideshow(bool favoritesOnly)
{
    if (!album_)
        return;
    slideshowQueue_.clear();
    for (int index = 0; index < album_->photos.size(); ++index) {
        if (!favoritesOnly || favorites_.contains(album_->photos.at(index).number))
            slideshowQueue_.push_back(index);
    }
    if (slideshowQueue_.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("No favourites"), QStringLiteral("Select at least one photograph first."));
        return;
    }
    slideshowPosition_ = slideshowQueue_.indexOf(currentIndex_);
    if (slideshowPosition_ < 0)
        slideshowPosition_ = 0;
    currentIndex_ = slideshowQueue_.at(slideshowPosition_);
    showPage(Page::Viewer);
    loadPhoto(currentIndex_);
    slideshowTimer_.start();
    statusBar()->showMessage(QStringLiteral("Slide show playing · %1 seconds per photograph")
                                 .arg(slideshowTimer_.interval() / 1000));
}

void MainWindow::pauseSlideshow()
{
    if (slideshowTimer_.isActive()) {
        slideshowTimer_.stop();
        statusBar()->showMessage(QStringLiteral("Slide show paused"), 3000);
    }
}

void MainWindow::advanceSlideshow()
{
    if (slideshowQueue_.isEmpty()) {
        pauseSlideshow();
        return;
    }
    slideshowPosition_ = (slideshowPosition_ + 1) % slideshowQueue_.size();
    loadPhoto(slideshowQueue_.at(slideshowPosition_));
}

void MainWindow::setResolution(PcdDecoder::Resolution resolution)
{
    if (resolution_ == resolution)
        return;
    resolution_ = resolution;
    if (album_ && page_ == Page::Viewer)
        loadPhoto(currentIndex_);
}

void MainWindow::showDecodeError(const QString &message)
{
    QMessageBox::warning(this, QStringLiteral("Photo CD read error"), message);
}
