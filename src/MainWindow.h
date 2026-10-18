// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "PcdDecoder.h"
#include "PhotoCdAlbum.h"

#include <QHash>
#include <QMainWindow>
#include <QSet>
#include <QTimer>
#include <optional>

class QLabel;
class QAction;
class QStackedWidget;
class QToolButton;
class PhotoCanvas;
class QGridLayout;

class MainWindow final : public QMainWindow {
public:
    explicit MainWindow(const QString &initialPath = {});

private:
    enum class Page { Welcome, Viewer, Index };

    void createMenus();
    void createPages();
    void applyStyle();
    void discoverDisc();
    void openDiscDialog();
    void exportAllPhotos();
    void openDiscPath(const QString &path);
    void setAlbum(PhotoCdAlbum album);
    void showPage(Page page);
    void showWelcome();
    void showIndex(bool favoritesOnly = false);
    void clearIndex();
    void loadStartupArtwork();
    void loadPhoto(int index);
    void updatePhotoLabels();
    void toggleFavorite();
    void saveFavorites();
    void startSlideshow(bool favoritesOnly);
    void pauseSlideshow();
    void advanceSlideshow();
    void setResolution(PcdDecoder::Resolution resolution);
    void showDecodeError(const QString &message);

    std::optional<PhotoCdAlbum> album_;
    Page page_ = Page::Welcome;
    PcdDecoder::Resolution resolution_ = PcdDecoder::FourBase;
    QSet<int> favorites_;
    QHash<int, QImage> thumbnailCache_;
    QVector<int> slideshowQueue_;
    int currentIndex_ = 0;
    int slideshowPosition_ = 0;
    quint64 imageRequest_ = 0;
    bool favoritesIndex_ = false;

    QStackedWidget *pages_ = nullptr;
    QWidget *welcomePage_ = nullptr;
    QWidget *viewerPage_ = nullptr;
    QWidget *indexPage_ = nullptr;
    QLabel *albumLabel_ = nullptr;
    QLabel *startupLabel_ = nullptr;
    QLabel *welcomeStatus_ = nullptr;
    QLabel *photoTitle_ = nullptr;
    QLabel *photoCount_ = nullptr;
    QLabel *indexTitle_ = nullptr;
    QLabel *viewerMessage_ = nullptr;
    PhotoCanvas *photoView_ = nullptr;
    QGridLayout *thumbnailGrid_ = nullptr;
    QTimer slideshowTimer_;
    QToolButton *favoriteButton_ = nullptr;
    QAction *exportAllAction_ = nullptr;
};
