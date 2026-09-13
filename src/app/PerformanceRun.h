#pragma once
#include "ui/MainWindow.h"
#include "render/VulkanViewport.h"
#include "render/ScenePicking.h"
#include "scene/PerformanceScene.h"
#include "io/SceneFile.h"
#include <QApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>
#include <algorithm>

// Opt-in benchmark. Intervals measure submitted-frame cadence including Qt/present
// pacing; recording times measure CPU command construction, not GPU execution.
class PerformanceRun {
public:
    PerformanceRun(MainWindow& window, QApplication& app, QJsonObject& report)
        : window_(window), app_(app), report_(report) {}
    void start() {
        window_.resize(1600,1000);
        QElapsedTimer setup; setup.start();
        window_.setScene(n3d::performanceScene());
        report_["scene_setup_ms"]=double(setup.nsecsElapsed())/1e6;
        report_["objects"]=1000; report_["boxes"]=500; report_["cylinders"]=500;
#ifdef NDEBUG
        report_["configuration"]="Release";
#else
        report_["configuration"]="Debug";
#endif
        auto* view=window_.viewport();
        view->camera.target={0,.2f,0}; view->camera.distance=22;
        QObject::connect(view,&VulkanViewport::frameRecorded,&window_,[this](qint64 cpu) {
            const auto now=clock_.nsecsElapsed();
            if(measuring_ && previous_>0) { intervals_.push_back(double(now-previous_)/1e6); cpu_.push_back(double(cpu)/1e6); }
            previous_=now;
        });
        clock_.start(); view->continuousRendering=true; view->requestUpdate();
        QTimer::singleShot(2000,&window_,[this] { beginSample(); });
        QTimer::singleShot(30000,&window_,[this] {
            if(!finished_) { window_.viewport()->continuousRendering=false; report_["error"]="benchmark timeout"; app_.exit(6); }
        });
    }
private:
    static double percentile(std::vector<double> values,double fraction) {
        if(values.empty()) return 0;
        std::sort(values.begin(),values.end());
        return values[size_t(std::ceil(fraction*values.size()))-1];
    }
    void beginSample() {
        intervals_.clear(); cpu_.clear(); previous_=0; measuring_=true;
        sampleStart_=clock_.nsecsElapsed();
        QTimer::singleShot(6000,&window_,[this] { endSample(); });
    }
    void endSample() {
        measuring_=false;
        auto* view=window_.viewport();
        const double seconds=double(clock_.nsecsElapsed()-sampleStart_)/1e9;
        QJsonObject sample;
        sample["mode"]=phase_==0 ? "perspective" : "orthographic";
        sample["seconds"]=seconds; sample["frames"]=int(intervals_.size());
        sample["submitted_fps"]=double(intervals_.size())/seconds;
        sample["frame_p50_ms"]=percentile(intervals_,.5);
        sample["frame_p95_ms"]=percentile(intervals_,.95);
        sample["cpu_record_p50_ms"]=percentile(cpu_,.5);
        sample["cpu_record_p95_ms"]=percentile(cpu_,.95);
        const bool integrated=view->physicalDeviceProperties()->deviceType==VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
        sample["target_fps"]=integrated ? 30 : 60;
        sample["target_met"]=double(intervals_.size())/seconds >= (integrated ? 30 : 60);
        samples_.append(sample);
        if(phase_++==0) {
            view->setProjection(true);
            QTimer::singleShot(2000,&window_,[this] { beginSample(); });
            return;
        }
        view->continuousRendering=false;
        report_["samples"]=samples_;
        report_["gpu"]=QString::fromUtf8(view->physicalDeviceProperties()->deviceName);
        const auto expected=app_.arguments().contains("--integrated") ? VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU : VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
        report_["device_matches_request"]=view->physicalDeviceProperties()->deviceType==expected;
        report_["width"]=view->swapChainImageSize().width(); report_["height"]=view->swapChainImageSize().height();
        report_["validation_enabled"]=view->vulkanInstance()->layers().contains("VK_LAYER_KHRONOS_validation");
        report_["timing_scope"]="CPU command recording and submitted-frame cadence; includes Qt/present pacing, not GPU timestamps";
        std::vector<double> picks;
        int hits=0;
        for(int i=0;i<40;++i) {
            const auto& object=window_.scene().objects()[size_t(i*25)];
            const auto p=MoveGizmo::project(view->modelViewProjection(),MoveGizmo::metres(object.data.positionMm)+QVector3D(0,.1f,0),view->size());
            QElapsedTimer timer; timer.start();
            const auto id=p ? ScenePicking::pick(window_.scene(),view->modelViewProjection(),*p,view->size()) : 0;
            picks.push_back(double(timer.nsecsElapsed())/1e6); hits+=id!=0;
        }
        report_["pick_p50_ms"]=percentile(picks,.5); report_["pick_p95_ms"]=percentile(picks,.95);
        report_["pick_hits"]=hits;
        bool roundTrip=false;
        try {
            QElapsedTimer timer; timer.start();
            SceneFile::write("build/performance-1000.n3d",window_.scene());
            report_["save_ms"]=double(timer.nsecsElapsed())/1e6;
            timer.restart();
            const auto loaded=SceneFile::read("build/performance-1000.n3d");
            report_["load_ms"]=double(timer.nsecsElapsed())/1e6;
            roundTrip=SceneFile::encode(loaded)==SceneFile::encode(window_.scene());
        } catch(const std::exception& error) { report_["file_error"]=QString::fromUtf8(error.what()); }
        report_["file_roundtrip"]=roundTrip;
        const auto pixels=view->grab();
        const QString gpu=app_.arguments().contains("--integrated") ? "intel" : "nvidia";
        const bool screenshot=!pixels.isNull() && pixels.save("build/performance-"+gpu+".png");
        report_["screenshot_saved"]=screenshot;
        report_["completed"]=intervals_.size()>30 && hits==40 && screenshot && roundTrip && report_["device_matches_request"].toBool();
        finished_=true;
        app_.exit(report_["completed"].toBool() ? 0 : 6);
    }
    MainWindow& window_; QApplication& app_; QJsonObject& report_;
    QElapsedTimer clock_; qint64 previous_=0, sampleStart_=0;
    bool measuring_=false,finished_=false; int phase_=0;
    std::vector<double> intervals_,cpu_; QJsonArray samples_;
};
