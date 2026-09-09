#include "ui/MainWindow.h"
#include "ui/RibbonCheck.h"
#include "ui/SceneCheck.h"
#include "ui/SelectionCheck.h"
#include "ui/DimensionCheck.h"
#include "ui/TransformCheck.h"
#include "ui/CreationCheck.h"
#include <cstdio>

#include <QApplication>
#include <QMenuBar>
#include <QStringList>
#include <QVulkanInstance>
#include <QMessageBox>
#include <QTimer>
#include <QFile>
#include <QTextStream>
#include "render/VulkanViewport.h"
#include "render/BoxImageCheck.h"
#include "render/GuideImageCheck.h"
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Nothing3D"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    QApplication::setOrganizationName(QStringLiteral("Nothing3D Learning Project"));

    const bool dimensionTest = application.arguments().contains(QStringLiteral("--dimension-test"));
    const bool transformTest = application.arguments().contains(QStringLiteral("--transform-test"));
    const bool smoke = transformTest || dimensionTest || application.arguments().contains(QStringLiteral("--smoke-test"));
    const bool gpuTest = application.arguments().contains(QStringLiteral("--gpu-test"));
    if (dimensionTest || transformTest) {
        qInstallMessageHandler([](QtMsgType, const QMessageLogContext&, const QString& text) {
            std::fprintf(stderr,"%s\n",qPrintable(text));
        });
    }
    if (gpuTest) {
        QFile freshLog(QStringLiteral("build/q3-vulkan.log"));
        if (!freshLog.open(QIODevice::WriteOnly | QIODevice::Truncate)) return 5;
        freshLog.close();
        qInstallMessageHandler([](QtMsgType, const QMessageLogContext&, const QString& text) {
            QFile log(QStringLiteral("build/q3-vulkan.log"));
            if (log.open(QIODevice::Append | QIODevice::Text)) {
                QTextStream out(&log);
                out << text << '\n';
            }
        });
    }
    int validationErrors = 0;
    // Construct before MainWindow so the instance outlives its Vulkan surface.
    QVulkanInstance instance;
    if (!smoke) {
        // Match glslc's explicit Vulkan 1.0 target instead of an unspecified version.
        instance.setApiVersion(QVersionNumber(1, 0, 0));
        if (instance.supportedLayers().contains("VK_LAYER_KHRONOS_validation"))
            instance.setLayers({"VK_LAYER_KHRONOS_validation"});
        instance.installDebugOutputFilter(QVulkanInstance::DebugUtilsFilter(
            [&validationErrors](auto severity, auto, const void*) {
                if (severity.testFlag(QVulkanInstance::ErrorSeverity)) ++validationErrors;
                return false;
            }));
        if (!instance.create()) {
            if (!gpuTest) QMessageBox::critical(nullptr, QStringLiteral("Vulkan 初始化失败"),
                QStringLiteral("无法初始化 Vulkan，错误码 %1。请检查显卡驱动。").arg(instance.errorCode()));
            return 2;
        }
    }
    int result = 0;
    {
    MainWindow window(smoke ? nullptr : &instance);
    if (dimensionTest) return checkDimensions(window) ? 0 : 1;
    if (transformTest) {
        window.show();
        QCoreApplication::processEvents();
        return checkTransforms(window) ? 0 : 1;
    }

    // The smoke-test path proves that Qt can construct the application and
    // main window without requiring a visible desktop window.
    if (application.arguments().contains(QStringLiteral("--smoke-test"))) {
        QImage blank(300, 300, QImage::Format_RGB32);
        blank.fill(QColor(10, 23, 41));
        OrbitCamera camera;
        QMatrix4x4 correction;
        correction(1,1)=-1; correction(2,2)=0.5f; correction(2,3)=0.5f;
        return window.windowTitle() == QStringLiteral("Nothing3D")
            && window.findChildren<QMenuBar*>().isEmpty()
            && window.findChild<QTabWidget*>("ribbonTabs") != nullptr
            && window.scene().objects().size() == 2
            && window.findChild<QTreeWidget*>("sceneTree")->topLevelItemCount() == 2
            && !hasBox(blank, correction*camera.projection(1)*camera.view()) ? 0 : 1;
    }

    bool visualChecks = true;
    int imageChecks = 0;
    auto checkImage = [&] {
        auto* viewport = window.viewport();
        if (!viewport || !viewport->isValid() || !viewport->supportsGrab()) { visualChecks=false; return; }
        const auto image = viewport->grab();
        const auto corner=Guides::compassRect(image.size(),viewport->devicePixelRatio());
        const bool boxOkay = hasBox(image, viewport->modelViewProjection(),corner);
        const bool guidesOkay = hasGuides(image,corner,Guides::compassMatrix(viewport->camera.view(),viewport->clipCorrectionMatrix()));
        qInfo("Image %d: box=%d guides=%d",imageChecks+1,int(boxOkay),int(guidesOkay));
        const bool okay = boxOkay && guidesOkay;
        visualChecks = visualChecks && okay;
        ++imageChecks;
        image.save(QStringLiteral("build/q3-check-%1.png").arg(imageChecks));
    };
    window.show();
    if (gpuTest) {
        auto* redraw = new QTimer(&window);
        QObject::connect(redraw, &QTimer::timeout, &window, [&window] {
            if (auto* viewport = window.viewport()) viewport->requestUpdate();
        });
        redraw->start(16);
        QTimer::singleShot(1000, &window, checkImage);
        QTimer::singleShot(1400, &window, [&] {
            auto* viewport=window.viewport();
            QMouseEvent press(QEvent::MouseButtonPress, QPointF(200,200), QPointF(200,200), Qt::MiddleButton, Qt::MiddleButton, Qt::NoModifier);
            QCoreApplication::sendEvent(viewport, &press);
            QMouseEvent move(QEvent::MouseMove, QPointF(290,220), QPointF(290,220), Qt::NoButton, Qt::MiddleButton, Qt::NoModifier);
            QCoreApplication::sendEvent(viewport, &move);
            QMouseEvent release(QEvent::MouseButtonRelease, QPointF(290,220), QPointF(290,220), Qt::MiddleButton, Qt::NoButton, Qt::NoModifier);
            QCoreApplication::sendEvent(viewport, &release);
            visualChecks = visualChecks && viewport->camera.yaw != 35 && viewport->camera.pitch != 25;
            QWheelEvent wheel(QPointF(200,200), QPointF(200,200), QPoint(), QPoint(0,120), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
            QCoreApplication::sendEvent(viewport, &wheel);
            visualChecks = visualChecks && viewport->camera.distance < 5.5f;
        });
        QTimer::singleShot(2000, &window, [&window] { window.resize(1100, 680); });
        QTimer::singleShot(2800, &window, checkImage);
        QTimer::singleShot(3500, &window, [&window] { window.showMinimized(); });
        QTimer::singleShot(4500, &window, [&window] { window.showNormal(); });
        QTimer::singleShot(4800, &window, [&window] { window.viewport()->toggleRotation(); });
        QTimer::singleShot(5600, &window, [&] {
            window.viewport()->toggleRotation();
            visualChecks = visualChecks && window.viewport()->modelAngle > 5;
            checkImage();
        });
        QTimer::singleShot(6000, &window, [&] {
            QKeyEvent reset(QEvent::KeyPress, Qt::Key_F, Qt::NoModifier);
            QCoreApplication::sendEvent(window.viewport(), &reset);
            visualChecks = visualChecks && window.viewport()->camera.yaw == 35 && window.viewport()->modelAngle == 0;
        });
        QTimer::singleShot(6300, &window, [&] {
            window.viewport()->camera.target=QVector3D(50,0.75f,50);
            window.viewport()->requestUpdate();
        });
        QTimer::singleShot(7000, &window, [&] {
            auto* viewport=window.viewport();
            const auto image=viewport->grab();
            const bool extended=hasInfiniteGrid(image,Guides::compassRect(image.size(),viewport->devicePixelRatio()));
            visualChecks &= extended;
            qInfo("Grid at (50m,50m): %d",int(extended));
            image.save(QStringLiteral("build/q3-infinite-grid.png"));
            const auto rect=Guides::compassRect(viewport->swapChainImageSize(),viewport->devicePixelRatio());
            const QPointF home=QPointF(rect.x()+rect.width()*0.89,rect.y()+rect.height()*0.10)/viewport->devicePixelRatio();
            QMouseEvent click(QEvent::MouseButtonPress,home,home,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
            QCoreApplication::sendEvent(viewport,&click);
            visualChecks &= (viewport->camera.target-QVector3D(0,0.75f,0)).length()<0.00001f;
        });
        QTimer::singleShot(7700, &window, [&, redraw] {
            redraw->stop();
            visualChecks = checkRibbon(window,window.viewport()) && visualChecks;
            visualChecks = checkSceneExamples(window) && visualChecks;
            visualChecks = checkSelection(window) && visualChecks;
            visualChecks = checkDimensions(window) && visualChecks;
            visualChecks = checkTransforms(window) && visualChecks;
            visualChecks = checkCreation(window) && visualChecks;
            auto* viewport = window.viewport();
            bool passed = viewport && viewport->isValid() && viewport->renderedFrames >= 100
                && visualChecks && imageChecks == 3
                && viewport->swapchainGenerations > 1
                && instance.layers().contains("VK_LAYER_KHRONOS_validation");
            if (viewport && viewport->isValid()) {
                const auto expected = application.arguments().contains(QStringLiteral("--integrated"))
                    ? VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU : VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
                passed = passed && viewport->physicalDeviceProperties()->deviceType == expected;
            }
            if (viewport && viewport->isValid() && viewport->supportsGrab()) {
                auto pixels = viewport->grab();
                passed = passed && !pixels.isNull();
                if (!pixels.isNull()) {
                    const auto corner=Guides::compassRect(pixels.size(),viewport->devicePixelRatio());
                    passed = passed && hasBox(pixels, viewport->modelViewProjection(),corner)
                        && hasGuides(pixels,corner,Guides::compassMatrix(viewport->camera.view(),viewport->clipCorrectionMatrix()));
                    pixels.save(QStringLiteral("build/q3-viewport.png"));
                }
            } else passed = false;
            window.grab().save(QStringLiteral("build/q3-window.png"));
            QFile report(QStringLiteral("build/q3-gpu-test.txt"));
            if (report.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&report);
                out << "Render/resize/readback checks: " << (passed ? "PASS" : "FAIL") << '\n';
                out << "reference_images=" << imageChecks << " camera/model/input_checks=" << visualChecks << '\n';
                if (viewport && viewport->isValid())
                    out << viewport->physicalDeviceProperties()->deviceName << '\n'
                        << "frames=" << viewport->renderedFrames << " swapchains=" << viewport->swapchainGenerations << '\n';
            }
            application.exit(passed ? 0 : 3);
        });
    }
    result = application.exec();
    }
    instance.destroy();
    if (gpuTest) {
        QFile report(QStringLiteral("build/q3-gpu-test.txt"));
        if (report.open(QIODevice::Append | QIODevice::Text)) {
            QTextStream out(&report);
            out << "validation_errors_including_shutdown=" << validationErrors << '\n';
            out << "Overall: " << ((result == 0 && validationErrors == 0) ? "PASS" : "FAIL") << '\n';
        }
    }
    return validationErrors ? 4 : result;
}
