#include "SimulationParametersMainWindow.h"

#include <Fonts/IconsFontAwesome5.h>

#include <Base/StringHelper.h>

#include <EngineInterface/LocationHelper.h>
#include <EngineInterface/ParametersEditService.h>
#include <EngineInterface/SimulationFacade.h>

#include <PersisterInterface/SerializerService.h>

#include "AlienGui.h"
#include "GenericFileDialog.h"
#include "GenericMessageDialog.h"
#include "LocationController.h"
#include "OverlayController.h"
#include "SimulationParametersLayerWidget.h"
#include "SimulationParametersSourceWidget.h"
#include "SpecificationGuiService.h"
#include "Viewport.h"

#include <ImFileDialog.h>
#include <EngineInterface/SimulationFacade.h>

namespace
{
    auto constexpr MasterHeight = 130.0f;
    auto constexpr MasterMinHeight = 50.0f;
    auto constexpr MasterRowHeight = 25.0f;

    auto constexpr DetailWidgetMinHeight = 0.0f;

    auto constexpr ExpertWidgetHeight = 130.0f;
    auto constexpr ExpertWidgetMinHeight = 60.0f;
}

SimulationParametersMainWindow::SimulationParametersMainWindow()
    : AlienWindow("시뮬레이션 설정", "windows.simulation parameters", false, true)
{}

void SimulationParametersMainWindow::initIntern()
{

    _masterWidgetOpen = GlobalSettings::get().getValue("windows.simulation parameters.master widget.open", _masterWidgetOpen);
    _detailWidgetOpen = GlobalSettings::get().getValue("windows.simulation parameters.detail widget.open", _detailWidgetOpen);
    _expertWidgetOpen = GlobalSettings::get().getValue("windows.simulation parameters.expert widget.open", _expertWidgetOpen);
    _masterWidgetHeight = GlobalSettings::get().getValue("windows.simulation parameters.master widget.height", scale(MasterHeight));
    _expertWidgetHeight = GlobalSettings::get().getValue("windows.simulation parameters.expert widget height", scale(ExpertWidgetHeight));

    auto baseWidgets = std::make_shared<_SimulationParametersBaseWidget>();
    baseWidgets->init();
    _baseWidgets = baseWidgets;

    auto layerWidgets = std::make_shared<_SimulationParameterLayerWidget>();
    layerWidgets->init(0);
    _layerWidgets = layerWidgets;


    auto sourceWidgets = std::make_shared<_SimulationParametersSourceWidgets>();
    sourceWidgets->init(0);
    _sourceWidgets = sourceWidgets;
}

void SimulationParametersMainWindow::processIntern()
{
    if (!_sessionId.has_value() || _sessionId.value() != _SimulationFacade::get()->getSessionId()) {
        _selectedOrderNumber = 0;
    }

    processToolbar();

    if (ImGui::BeginChild("##content", {0, -scale(50.0f)})) {

        updateLocations();

        auto origMasterHeight = _masterWidgetHeight;
        auto origExpertWidgetHeight = _expertWidgetHeight;

        processMasterWidget();
        processDetailWidget();
        processExpertWidget();

        correctLayout(origMasterHeight, origExpertWidgetHeight);
    }
    }
    ImGui::EndChild();

    processStatusBar();

    _sessionId = _SimulationFacade::get()->getSessionId();
}

void SimulationParametersMainWindow::shutdownIntern()
{
    GlobalSettings::get().setValue("windows.simulation parameters.master widget.open", _masterWidgetOpen);
    GlobalSettings::get().setValue("windows.simulation parameters.detail widget.open", _detailWidgetOpen);
    GlobalSettings::get().setValue("windows.simulation parameters.expert widget.open", _expertWidgetOpen);
    GlobalSettings::get().setValue("windows.simulation parameters.master widget.height", _masterWidgetHeight);
    GlobalSettings::get().setValue("windows.simulation parameters.expert widget height", _expertWidgetHeight);
}

void SimulationParametersMainWindow::processToolbar()
{
    if (AlienGui::ToolbarButton(AlienGui::ToolbarButtonParameters().text(ICON_FA_FOLDER_OPEN).tooltip("파일에서 설정 불러오기"))) {
        onOpenParameters();
    }

    ImGui::SameLine();
    if (AlienGui::ToolbarButton(AlienGui::ToolbarButtonParameters().text(ICON_FA_SAVE).tooltip("설정을 파일로 저장하기"))) {
        onSaveParameters();
    }

    ImGui::SameLine();
    AlienGui::ToolbarSeparator();

    ImGui::SameLine();
    if (AlienGui::ToolbarButton(AlienGui::ToolbarButtonParameters().text(ICON_FA_COPY).tooltip("설정을 클립보드에 복사"))) {
        _copiedParameters = _SimulationFacade::get()->getSimulationParameters();
        printOverlayMessage("시뮬레이션 설정이 복사되었습니다");
    }

    ImGui::SameLine();
    if (AlienGui::ToolbarButton(
            AlienGui::ToolbarButtonParameters().text(ICON_FA_PASTE).tooltip("클립보드 설정을 붙여넣기").disabled(!_copiedParameters))) {
        _SimulationFacade::get()->setSimulationParameters(*_copiedParameters);
        _SimulationFacade::get()->setOriginalSimulationParameters(*_copiedParameters);
        printOverlayMessage("시뮬레이션 설정이 붙여넣어졌습니다");
    }

    ImGui::SameLine();
    if (AlienGui::ToolbarButton(AlienGui::ToolbarButtonParameters()
                                    .text(ICON_FA_PASTE)
                                    .secondText(ICON_FA_UNDO)
                                    .secondTextOffset(RealVector2D{32.0f, 28.0f})
                                    .secondTextScale(0.3f)
                                    .tooltip("기준값을 클립보드의 값으로 교체합니다. 현재 설정과 클립보드 설정 사이의 차이점을 확인하는 데 유용합니다.")
                                    .disabled(!_copiedParameters))) {
        auto parameters = _SimulationFacade::get()->getSimulationParameters();
        if (_copiedParameters->numLayers == parameters.numLayers && _copiedParameters->numSources == parameters.numSources) {
            _SimulationFacade::get()->setOriginalSimulationParameters(*_copiedParameters);
            printOverlayMessage("기준 시뮬레이션 설정이 교체되었습니다");
        } else {
            GenericMessageDialog::get().information(
                "오류", "현재 설정의 레이어 및 방사능 광원 개수가 클립보드와 일치해야 합니다.");
        }
    }

    ImGui::SameLine();
    AlienGui::ToolbarSeparator();

    ImGui::SameLine();
    if (AlienGui::ToolbarButton(AlienGui::ToolbarButtonParameters().text(ICON_FA_PLUS).secondText(ICON_FA_LAYER_GROUP).tooltip("파라미터 레이어 추가"))) {
        onInsertDefaultLayer();
    }

    ImGui::SameLine();
    if (AlienGui::ToolbarButton(AlienGui::ToolbarButtonParameters().text(ICON_FA_PLUS).secondText(ICON_FA_SUN).tooltip("방사능 광원 추가"))) {
        onInsertDefaultSource();
    }

    ImGui::SameLine();
    if (AlienGui::ToolbarButton(AlienGui::ToolbarButtonParameters()
                                    .text(ICON_FA_PLUS)
                                    .secondText(ICON_FA_CLONE)
                                    .disabled(_selectedOrderNumber == 0)
                                    .tooltip("선택한 레이어/광원 복제"))) {
        onCloneLocation();
    }

    ImGui::SameLine();
    if (AlienGui::ToolbarButton(
            AlienGui::ToolbarButtonParameters().text(ICON_FA_MINUS).disabled(_selectedOrderNumber == 0).tooltip("선택한 레이어/광원 삭제"))) {
        onDeleteLocation();
    }

    ImGui::SameLine();
    AlienGui::ToolbarSeparator();

    ImGui::SameLine();
    if (AlienGui::ToolbarButton(AlienGui::ToolbarButtonParameters()
                                    .text(ICON_FA_CHEVRON_UP)
                                    .disabled(_selectedOrderNumber <= 1)
                                    .tooltip("선택한 레이어/광원을 위로 이동"))) {
        onDecreaseOrderNumber();
    }

    ImGui::SameLine();
    if (AlienGui::ToolbarButton(AlienGui::ToolbarButtonParameters()
                                    .text(ICON_FA_CHEVRON_DOWN)
                                    .tooltip("선택한 레이어/광원을 아래로 이동")
                                    .disabled(_selectedOrderNumber >= _locations.size() - 1 || _selectedOrderNumber == 0))) {
        onIncreaseOrderNumber();
    }

    ImGui::SameLine();
    AlienGui::ToolbarSeparator();

    ImGui::SameLine();
    if (AlienGui::ToolbarButton(AlienGui::ToolbarButtonParameters()
                                    .text(ICON_FA_EXTERNAL_LINK_SQUARE_ALT)
                                    .tooltip("선택한 레이어/광원의 설정을 새 창에서 열기"))) {
        onOpenInLocationWindow();
    }

    AlienGui::Separator();
}

void SimulationParametersMainWindow::processMasterWidget()
{
    if (ImGui::BeginChild("##master", {0, getMasterWidgetHeight()})) {

        if (_masterWidgetOpen =
                AlienGui::BeginTreeNode(AlienGui::TreeNodeParameters().name("전체 개요").rank(AlienGui::TreeNodeRank::High).defaultOpen(_masterWidgetOpen))) {
            ImGui::Spacing();
            if (ImGui::BeginChild("##master2", {0, -ImGui::GetStyle().FramePadding.y})) {
                processLocationTable();
            }
            ImGui::EndChild();
        }
        AlienGui::EndTreeNode();
    }
    ImGui::EndChild();

    if (_masterWidgetOpen && (_detailWidgetOpen || _expertWidgetOpen)) {
        ImGui::PushID("master");
        AlienGui::MovableHorizontalSeparator(AlienGui::MovableHorizontalSeparatorParameters(), _masterWidgetHeight);
        ImGui::PopID();
    }
}

void SimulationParametersMainWindow::processDetailWidget()
{
    auto height = getDetailWidgetHeight();
    if (ImGui::BeginChild("##detail", {0, height})) {
        auto title = _filter.empty() ? "세부 설정" : "세부 설정 (필터링됨)";
        if (_detailWidgetOpen = AlienGui::BeginTreeNode(AlienGui::TreeNodeParameters()
                                                            .name((std::string(title) + "###parameters").c_str())
                                                            .rank(AlienGui::TreeNodeRank::High)
                                                            .defaultOpen(_detailWidgetOpen))) {
            ImGui::Spacing();
            if (ImGui::BeginChild(
                    "##detail2", {0, -ImGui::GetStyle().FramePadding.y - scale(33.0f)}, ImGuiChildFlags_Border, ImGuiWindowFlags_HorizontalScrollbar)) {
                auto type = _locations.at(_selectedOrderNumber).type;
                ParametersFilter filter{.containedText = _filter};
                if (type == LocationType::Base) {
                    _baseWidgets->process(filter);
                } else if (type == LocationType::Layer) {
                    _layerWidgets->setOrderNumber(_selectedOrderNumber);
                    _layerWidgets->process(filter);
                } else if (type == LocationType::Source) {
                    _sourceWidgets->setOrderNumber(_selectedOrderNumber);
                    _sourceWidgets->process(filter);
                }
            }
            ImGui::EndChild();

            ImGui::Spacing();
            AlienGui::InputFilter(AlienGui::InputFilterParameters().width(250.0f), _filter);
        }
        AlienGui::EndTreeNode();
    }
    ImGui::EndChild();

    if (_detailWidgetOpen && _expertWidgetOpen) {
        ImGui::PushID("detail");
        AlienGui::MovableHorizontalSeparator(AlienGui::MovableHorizontalSeparatorParameters().additive(false), _expertWidgetHeight);
        ImGui::PopID();
    }
}

void SimulationParametersMainWindow::processExpertWidget()
{
    if (ImGui::BeginChild("##expert", {0, 0})) {
        if (_expertWidgetOpen = AlienGui::BeginTreeNode(
                AlienGui::TreeNodeParameters().name("전문가 설정").rank(AlienGui::TreeNodeRank::High).defaultOpen(_expertWidgetOpen))) {
            if (ImGui::BeginChild("##expert2", {0, 0}, ImGuiChildFlags_Border, ImGuiWindowFlags_HorizontalScrollbar)) {
                processExpertSettings();
            }
            ImGui::EndChild();
        }
        AlienGui::EndTreeNode();
    }
    ImGui::EndChild();
}

void SimulationParametersMainWindow::processStatusBar()
{
    std::vector<std::string> statusItems;
    statusItems.emplace_back("정확한 값을 입력하려면 슬라이더를 CTRL + 클릭하세요");

    AlienGui::StatusBar(statusItems);
}

void SimulationParametersMainWindow::processLocationTable()
{
    static ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_RowBg
        | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_ScrollY | ImGuiTableFlags_ScrollX;

    if (ImGui::BeginTable("Locations", 5, flags, ImVec2(-1, -1), 0)) {

        ImGui::TableSetupColumn("이름", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed, scale(140.0f));
        ImGui::TableSetupColumn("유형", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed, scale(140.0f));
        ImGui::TableSetupColumn("위치", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthFixed, scale(115.0f));
        ImGui::TableSetupColumn("강도", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthFixed, scale(90.0f));
        ImGui::TableSetupColumn("불투명도", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthFixed, scale(90.0f));
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();
        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, Const::TableHeaderColor);

        ImGuiListClipper clipper;
        clipper.Begin(toInt(_locations.size()));
        while (clipper.Step()) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
                auto const& entry = _locations.at(row);

                ImGui::PushID(row);
                ImGui::TableNextRow(0, scale(MasterRowHeight));

                ImGui::TableNextColumn();
                auto selected = _selectedOrderNumber == row;
                if (ImGui::Selectable(
                        "",
                        &selected,
                        ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap,
                        ImVec2(0, scale(MasterRowHeight) - ImGui::GetStyle().FramePadding.y))) {
                    _selectedOrderNumber = row;
                }
                ImGui::SameLine();
                std::string icon;
                if (entry.type == LocationType::Base) {
                    icon = "";
                } else if (entry.type == LocationType::Layer) {
                    icon = ICON_FA_LAYER_GROUP " ";
                } else if (entry.type == LocationType::Source) {
                    icon = ICON_FA_SUN " ";
                }
                AlienGui::Text(icon + entry.name);


                ImGui::TableNextColumn();
                if (entry.type == LocationType::Base) {
                    AlienGui::Text("기본 파라미터");
                } else if (entry.type == LocationType::Layer) {
                    AlienGui::Text("레이어");
                } else if (entry.type == LocationType::Source) {
                    AlienGui::Text("방사능 광원");
                }

                ImGui::TableNextColumn();
                if (row > 0) {
                    if (AlienGui::ActionButton(AlienGui::ActionButtonParameters().buttonText(ICON_FA_SEARCH))) {
                        onCenterLocation(row);
                        _selectedOrderNumber = row;
                    }
                    ImGui::SameLine();
                }
                AlienGui::Text(entry.position);

                ImGui::TableNextColumn();
                if (entry.type == LocationType::Base || entry.type == LocationType::Source) {
                    AlienGui::Text(entry.strength);
                } else {
                    AlienGui::Text("-");
                }

                ImGui::TableNextColumn();
                if (entry.type == LocationType::Layer) {
                    AlienGui::Text(entry.strength);
                } else {
                    AlienGui::Text("-");
                }

                ImGui::PopID();
            }
        }
        ImGui::EndTable();
    }
}

void SimulationParametersMainWindow::processExpertSettings()
{
    auto parameters = _SimulationFacade::get()->getSimulationParameters();
    auto origParameters = _SimulationFacade::get()->getOriginalSimulationParameters();
    auto lastParameters = parameters;

    SpecificationGuiService::get().createWidgetsForExpertToggles(parameters, origParameters);

    if (parameters != lastParameters) {
        _SimulationFacade::get()->setSimulationParameters(parameters);
    }
}

void SimulationParametersMainWindow::onOpenParameters()
{
    GenericFileDialog::get().showOpenFileDialog(
        "시뮬레이션 설정 불러오기", "Simulation parameters (*.parameters){.parameters},.*", _fileDialogPath, [&](std::filesystem::path const& path) {
            auto firstFilename = ifd::FileDialog::Instance().GetResult();
            auto firstFilenameCopy = firstFilename;
            _fileDialogPath = firstFilenameCopy.remove_filename().string();

            SimulationParameters parameters;
            if (!SerializerService::get().deserializeSimulationParametersFromFile(parameters, firstFilename.string())) {
                GenericMessageDialog::get().information("시뮬레이션 설정 불러오기", "선택한 파일을 열 수 없습니다.");
            } else {
                _SimulationFacade::get()->setSimulationParameters(parameters);
                _SimulationFacade::get()->setOriginalSimulationParameters(parameters);
            }
        });
}

void SimulationParametersMainWindow::onSaveParameters()
{
    GenericFileDialog::get().showSaveFileDialog(
        "시뮬레이션 설정 저장하기", "Simulation parameters (*.parameters){.parameters},.*", _fileDialogPath, [&](std::filesystem::path const& path) {
            auto firstFilename = ifd::FileDialog::Instance().GetResult();
            auto firstFilenameCopy = firstFilename;
            _fileDialogPath = firstFilenameCopy.remove_filename().string();

            auto parameters = _SimulationFacade::get()->getSimulationParameters();
            if (!SerializerService::get().serializeSimulationParametersToFile(firstFilename.string(), parameters)) {
                GenericMessageDialog::get().information("시뮬레이션 설정 저장하기", "선택한 파일을 저장할 수 없습니다.");
            }
        });
}

void SimulationParametersMainWindow::onInsertDefaultLayer()
{
    auto& editService = ParametersEditService::get();
    auto parameters = _SimulationFacade::get()->getSimulationParameters();
    auto origParameters = _SimulationFacade::get()->getOriginalSimulationParameters();

    if (!checkNumLayers(parameters)) {
        return;
    }

    auto newByOldOrderNumber = editService.insertDefaultLayer(parameters, _selectedOrderNumber);
    editService.insertDefaultLayer(origParameters, _selectedOrderNumber);

    ++_selectedOrderNumber;

    auto worldSize = _SimulationFacade::get()->getWorldSize();
    auto minRadius = toFloat(std::min(worldSize.x, worldSize.y)) / 2;

    auto index = LocationHelper::findLocationArrayIndex(parameters, _selectedOrderNumber);
    parameters.backgroundColor.layerValues[index].enabled = true;
    parameters.backgroundColor.layerValues[index].value = _layerColorPalette.getColor((2 + parameters.numLayers) * 8);
    parameters.layerShape.layerValues[index] = LayerShapeType_Circular;
    parameters.layerPosition.layerValues[index] = {
        toFloat(worldSize.x / 2 + (_insertedLocationCounter % 10) * worldSize.x / 20),
        toFloat(worldSize.y / 2 + (_insertedLocationCounter % 10) * worldSize.y / 20)};
    parameters.layerCoreRadius.layerValues[index] = minRadius / 3;
    parameters.layerCoreRect.layerValues[index] = {minRadius / 3, minRadius / 3};
    parameters.layerFadeoutRadius.layerValues[index] = minRadius / 5;
    parameters.layerRadialForceFieldOrientation.layerValues[index] = Orientation_Clockwise;
    parameters.layerRadialForceFieldStrength.layerValues[index] = 0.001f;
    parameters.layerRadialForceFieldDriftAngle.layerValues[index] = 0.0f;

    origParameters.backgroundColor.layerValues[index] = parameters.backgroundColor.layerValues[index];
    origParameters.layerShape.layerValues[index] = parameters.layerShape.layerValues[index];
    origParameters.layerPosition.layerValues[index] = parameters.layerPosition.layerValues[index];
    origParameters.layerCoreRadius.layerValues[index] = parameters.layerCoreRadius.layerValues[index];
    origParameters.layerCoreRect.layerValues[index] = parameters.layerCoreRect.layerValues[index];
    origParameters.layerFadeoutRadius.layerValues[index] = parameters.layerFadeoutRadius.layerValues[index];
    origParameters.layerForceFieldType.layerValues[index] = parameters.layerForceFieldType.layerValues[index];
    origParameters.layerRadialForceFieldOrientation.layerValues[index] = parameters.layerRadialForceFieldOrientation.layerValues[index];
    origParameters.layerRadialForceFieldStrength.layerValues[index] = parameters.layerRadialForceFieldStrength.layerValues[index];
    origParameters.layerRadialForceFieldDriftAngle.layerValues[index] = parameters.layerRadialForceFieldDriftAngle.layerValues[index];

    _SimulationFacade::get()->setSimulationParameters(parameters);
    _SimulationFacade::get()->setOriginalSimulationParameters(origParameters);

    LocationController::get().remapLocationIndices(newByOldOrderNumber);
    ++_insertedLocationCounter;
}

void SimulationParametersMainWindow::onInsertDefaultSource()
{
    auto& editService = ParametersEditService::get();
    auto parameters = _SimulationFacade::get()->getSimulationParameters();
    auto origParameters = _SimulationFacade::get()->getOriginalSimulationParameters();

    if (!checkNumSources(parameters)) {
        return;
    }
    auto strengths = editService.getRadiationStrengths(parameters);
    auto newStrengths = editService.calcRadiationStrengthsForAddingSource(strengths);

    auto newByOldOrderNumber = editService.insertDefaultSource(parameters, _selectedOrderNumber);
    editService.insertDefaultSource(origParameters, _selectedOrderNumber);

    ++_selectedOrderNumber;

    editService.applyRadiationStrengths(parameters, newStrengths);
    editService.applyRadiationStrengths(origParameters, newStrengths);

    auto index = LocationHelper::findLocationArrayIndex(parameters, _selectedOrderNumber);
    auto worldSize = _SimulationFacade::get()->getWorldSize();
    parameters.sourcePosition.sourceValues[index] = {
        toFloat(worldSize.x / 2 + (_insertedLocationCounter % 10) * worldSize.x / 20),
        toFloat(worldSize.y / 2 + (_insertedLocationCounter % 10) * worldSize.y / 20)};
    origParameters.sourcePosition.sourceValues[index] = parameters.sourcePosition.sourceValues[index];

    _SimulationFacade::get()->setSimulationParameters(parameters);
    _SimulationFacade::get()->setOriginalSimulationParameters(origParameters);

    LocationController::get().remapLocationIndices(newByOldOrderNumber);
    ++_insertedLocationCounter;
}

void SimulationParametersMainWindow::onCloneLocation()
{
    auto& editService = ParametersEditService::get();
    auto parameters = _SimulationFacade::get()->getSimulationParameters();
    auto origParameters = _SimulationFacade::get()->getOriginalSimulationParameters();

    auto locationType = LocationHelper::getLocationType(_selectedOrderNumber, parameters);
    if (locationType == LocationType::Layer) {
        if (!checkNumLayers(parameters)) {
            return;
        }
    } else {
        if (!checkNumSources(parameters)) {
            return;
        }
    }

    auto strengths = editService.getRadiationStrengths(parameters);
    auto newStrengths = editService.calcRadiationStrengthsForAddingSource(strengths);

    auto newByOldOrderNumber = editService.cloneLocation(parameters, _selectedOrderNumber);
    editService.cloneLocation(origParameters, _selectedOrderNumber);

    if (locationType == LocationType::Source) {
        editService.applyRadiationStrengths(parameters, newStrengths);
        editService.applyRadiationStrengths(origParameters, newStrengths);
    }

    ++_selectedOrderNumber;
    _SimulationFacade::get()->setSimulationParameters(parameters);
    _SimulationFacade::get()->setOriginalSimulationParameters(origParameters);

    LocationController::get().remapLocationIndices(newByOldOrderNumber);
}

void SimulationParametersMainWindow::onDeleteLocation()
{
    auto& editService = ParametersEditService::get();
    auto parameters = _SimulationFacade::get()->getSimulationParameters();
    auto origParameters = _SimulationFacade::get()->getOriginalSimulationParameters();

    LocationController::get().deleteLocationWindow(_selectedOrderNumber);

    auto newByOldOrderNumber = editService.deleteLocation(parameters, _selectedOrderNumber);
    editService.deleteLocation(origParameters, _selectedOrderNumber);

    if (_locations.size() - 1 == _selectedOrderNumber) {
        --_selectedOrderNumber;
    }

    _SimulationFacade::get()->setSimulationParameters(parameters);
    _SimulationFacade::get()->setOriginalSimulationParameters(origParameters);

    LocationController::get().remapLocationIndices(newByOldOrderNumber);
}

void SimulationParametersMainWindow::onDecreaseOrderNumber()
{
    auto& editService = ParametersEditService::get();
    auto parameters = _SimulationFacade::get()->getSimulationParameters();
    auto origParameters = _SimulationFacade::get()->getOriginalSimulationParameters();

    auto newByOldOrderNumber = editService.moveLocationUpwards(parameters, _selectedOrderNumber);
    editService.moveLocationUpwards(origParameters, _selectedOrderNumber);

    --_selectedOrderNumber;

    _SimulationFacade::get()->setSimulationParameters(parameters);
    _SimulationFacade::get()->setOriginalSimulationParameters(origParameters);

    LocationController::get().remapLocationIndices(newByOldOrderNumber);
}

void SimulationParametersMainWindow::onIncreaseOrderNumber()
{
    auto& editService = ParametersEditService::get();
    auto parameters = _SimulationFacade::get()->getSimulationParameters();
    auto origParameters = _SimulationFacade::get()->getOriginalSimulationParameters();

    auto newByOldOrderNumber = editService.moveLocationDownwards(parameters, _selectedOrderNumber);
    editService.moveLocationDownwards(origParameters, _selectedOrderNumber);

    ++_selectedOrderNumber;

    _SimulationFacade::get()->setSimulationParameters(parameters);
    _SimulationFacade::get()->setOriginalSimulationParameters(origParameters);

    LocationController::get().remapLocationIndices(newByOldOrderNumber);
}

void SimulationParametersMainWindow::onOpenInLocationWindow()
{
    auto mousePos = ImGui::GetMousePos();
    auto offset = RealVector2D{50.0f + toFloat(_locationWindowCounter) * 15, toFloat(_locationWindowCounter) * 15};
    LocationController::get().addLocationWindow(_selectedOrderNumber, {mousePos.x + offset.x, mousePos.y + offset.y});
    _locationWindowCounter = (_locationWindowCounter + 1) % 8;
}

void SimulationParametersMainWindow::onCenterLocation(int orderNumber)
{
    auto parameters = _SimulationFacade::get()->getSimulationParameters();
    auto locationType = LocationHelper::getLocationType(orderNumber, parameters);
    auto arrayIndex = LocationHelper::findLocationArrayIndex(parameters, orderNumber);
    RealVector2D pos;
    if (locationType == LocationType::Layer) {
        pos = parameters.layerPosition.layerValues[arrayIndex];
    } else if (locationType == LocationType::Source) {
        pos = parameters.sourcePosition.sourceValues[arrayIndex];
    }
    Viewport::get().setCenterInWorldPos(pos);
}

void SimulationParametersMainWindow::updateLocations()
{
    auto parameters = _SimulationFacade::get()->getSimulationParameters();

    _locations = std::vector<Location>(1 + parameters.numLayers + parameters.numSources);
    auto radiationStrength = ParametersEditService::get().getRadiationStrengths(parameters);
    auto pinnedString = radiationStrength.pinned.contains(0) ? ICON_FA_THUMBTACK " " : " ";
    _locations.at(0) = Location{"기본", LocationType::Base, "-", pinnedString + StringHelper::format(radiationStrength.values.front() * 100 + 0.05f, 1) + "%"};
    for (int i = 0; i < parameters.numLayers; ++i) {
        auto position = "(" + StringHelper::format(parameters.layerPosition.layerValues[i].x, 0) + ", "
            + StringHelper::format(parameters.layerPosition.layerValues[i].y, 0) + ")";
        _locations.at(parameters.layerOrderNumbers[i]) = Location{
            .name = parameters.layerName.layerValues[i],
            .type = LocationType::Layer,
            .position = position,
            .strength = " " + StringHelper::format(parameters.layerOpacity.layerValues[i] * 100 + 0.05f, 1) + "%"};
    }
    for (int i = 0; i < parameters.numSources; ++i) {
        auto position = "(" + StringHelper::format(parameters.sourcePosition.sourceValues[i].x, 0) + ", "
            + StringHelper::format(parameters.sourcePosition.sourceValues[i].y, 0) + ")";
        auto pinnedString = radiationStrength.pinned.contains(i + 1) ? ICON_FA_THUMBTACK " " : " ";
        _locations.at(parameters.sourceOrderNumbers[i]) = Location{
            parameters.sourceName.sourceValues[i],
            LocationType::Source,
            position,
            pinnedString + StringHelper::format(radiationStrength.values.at(i + 1) * 100 + 0.05f, 1) + "%"};
    }
}

void SimulationParametersMainWindow::correctLayout(float origMasterHeight, float origExpertWidgetHeight)
{
    auto detailHeight = ImGui::GetWindowSize().y - getMasterWidgetRefHeight() - getExpertWidgetRefHeight();

    if (detailHeight < scale(DetailWidgetMinHeight) || _masterWidgetHeight < scale(MasterMinHeight) || _expertWidgetHeight < scale(ExpertWidgetMinHeight)) {
        _masterWidgetHeight = origMasterHeight;
        _expertWidgetHeight = origExpertWidgetHeight;
    }
}

bool SimulationParametersMainWindow::checkNumLayers(SimulationParameters const& parameters)
{
    if (parameters.numLayers == MAX_LAYERS) {
        showMessage("오류", "레이어 최대 개수에 도달했습니다.");
        return false;
    }
    return true;
}

bool SimulationParametersMainWindow::checkNumSources(SimulationParameters const& parameters)
{
    if (parameters.numSources == MAX_SOURCES) {
        showMessage("오류", "방사능 광원 최대 개수에 도달했습니다.");
        return false;
    }
    return true;
}

float SimulationParametersMainWindow::getMasterWidgetRefHeight() const
{
    return _masterWidgetOpen ? _masterWidgetHeight : scale(25.0f);
}

float SimulationParametersMainWindow::getExpertWidgetRefHeight() const
{
    return _expertWidgetOpen ? _expertWidgetHeight : scale(30.0f);
}

float SimulationParametersMainWindow::getMasterWidgetHeight() const
{
    if (_masterWidgetOpen && !_detailWidgetOpen && !_expertWidgetOpen) {
        return std::max(scale(MasterMinHeight), ImGui::GetContentRegionAvail().y - getDetailWidgetHeight() - getExpertWidgetRefHeight());
    }
    return getMasterWidgetRefHeight();
}

float SimulationParametersMainWindow::getDetailWidgetHeight() const
{
    return _detailWidgetOpen ? std::max(scale(MasterMinHeight), ImGui::GetContentRegionAvail().y - getExpertWidgetRefHeight() + scale(4.0f)) : scale(25.0f);
}
