#include "ui/MainWindow.h"
#include "ui/UiLayoutCheck.h"
#include "ui/RibbonCheck.h"
#include "ui/SceneCheck.h"
#include "ui/SelectionCheck.h"
#include "ui/DimensionCheck.h"
#include "ui/TransformCheck.h"
#include "ui/CreationCheck.h"
#include "ui/MoveCheck.h"
#include "ui/HistoryCheck.h"
#include "ui/ObjectActionsCheck.h"
#include "ui/ProjectCheck.h"
#include "app/PerformanceRun.h"
#include <QJsonDocument>
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
#include <QPainter>

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Nothing3D"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    QApplication::setOrganizationName(QStringLiteral("Nothing3D Learning Project"));

    const bool dimensionTest = application.arguments().contains(QStringLiteral("--dimension-test"));
    const bool transformTest = application.arguments().contains(QStringLiteral("--transform-test"));
    const bool historyTest = application.arguments().contains(QStringLiteral("--history-test"));
    const bool objectActionsTest = application.arguments().contains(QStringLiteral("--object-actions-test"));
    const bool projectTest = application.arguments().contains(QStringLiteral("--project-test"));
    const bool uiLayoutTest = application.arguments().contains(QStringLiteral("--ui-layout-test"));
    const bool smoke = uiLayoutTest || projectTest || objectActionsTest || historyTest || transformTest || dimensionTest || application.arguments().contains(QStringLiteral("--smoke-test"));
    const bool gpuTest = application.arguments().contains(QStringLiteral("--gpu-test"));
    const bool performanceTest=application.arguments().contains(QStringLiteral("--performance-test"));
    if (projectTest || objectActionsTest || historyTest || dimensionTest || transformTest) {
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
        if ((!performanceTest || application.arguments().contains("--performance-validation"))
            && instance.supportedLayers().contains("VK_LAYER_KHRONOS_validation"))
            instance.setLayers({"VK_LAYER_KHRONOS_validation"});
        instance.installDebugOutputFilter(QVulkanInstance::DebugUtilsFilter(
            [&validationErrors](auto severity, auto, const void*) {
                if (severity.testFlag(QVulkanInstance::ErrorSeverity)) ++validationErrors;
                return false;
            }));
        if (!instance.create()) {
            if (!gpuTest && !performanceTest) QMessageBox::critical(nullptr, QStringLiteral("Vulkan 初始化失败"),
                QStringLiteral("无法初始化 Vulkan，错误码 %1。请检查显卡驱动。").arg(instance.errorCode()));
            return 2;
        }
    }
    int result = 0;
    QJsonObject performanceReport;
    {
    MainWindow window(smoke ? nullptr : &instance);
    PerformanceRun performance(window,application,performanceReport);
    if (projectTest) { window.show(); QCoreApplication::processEvents(); return checkProject(window)?0:1; }
    if (uiLayoutTest) return checkUiLayout(window)?0:1;
    if (objectActionsTest) { window.show(); QCoreApplication::processEvents(); return checkObjectActions(window)?0:1; }
    if (historyTest) { window.show(); QCoreApplication::processEvents(); return checkHistory(window)?0:1; }
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
    if(performanceTest) performance.start();
    if (application.arguments().contains(QStringLiteral("--ui-preview"))) {
        QTimer::singleShot(1800,&window,[&window,&application] {
            if (!window.scene().objects().empty()) window.selectObject(window.scene().objects().front().id);
            auto screenshot=window.grab();
            if (auto* view=window.viewport(); view && view->supportsGrab()) {
                const auto pixels=view->grab();
                const QPoint origin=window.mapFromGlobal(view->mapToGlobal(QPoint(0,0)));
                QPainter painter(&screenshot);
                painter.drawImage(QRect(origin,view->size()),pixels);
            }
            screenshot.save(QStringLiteral("build/ui-preview.png"));
            application.quit();
        });
    }
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
            // Preserve the historical solid-pixel reference and navigation tests
            // with the user-facing handle toggle off. MoveCheck below verifies
            // the overlay and interactions with handles enabled.
            window.findChild<QAction*>("showMoveGizmo")->setChecked(false);
            visualChecks = checkSelection(window) && visualChecks;
            visualChecks = checkDimensions(window) && visualChecks;
            visualChecks = checkTransforms(window) && visualChecks;
            visualChecks = checkCreation(window) && visualChecks;
            window.findChild<QAction*>("showMoveGizmo")->setChecked(true);
            visualChecks = checkMove(window) && visualChecks;
            visualChecks = checkHistory(window) && visualChecks;
            visualChecks = checkObjectActions(window) && visualChecks;
            visualChecks = checkProject(window) && visualChecks;
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
    if(performanceTest) {
        performanceReport["validation_errors_including_shutdown"]=validationErrors;
        performanceReport["exit_code"]=validationErrors ? 4 : result;
        QFile report(application.arguments().contains("--integrated") ? "build/performance-intel.json" : "build/performance-nvidia.json");
        if(!report.open(QIODevice::WriteOnly) || report.write(QJsonDocument(performanceReport).toJson())<0) return 7;
    }
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
