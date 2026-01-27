#include "SimulationParametersSourceWidget.h"

#include <imgui.h>

#include <EngineInterface/LocationHelper.h>
#include <EngineInterface/ParametersValidationService.h>
#include <EngineInterface/SimulationFacade.h>

#include "SimulationInteractionController.h"
#include "SpecificationGuiService.h"

void _SimulationParametersSourceWidgets::init(int orderNumber)
{
    _orderNumber = orderNumber;
}

void _SimulationParametersSourceWidgets::process(ParametersFilter const& filter)
{
    auto parameters = _SimulationFacade::get()->getSimulationParameters();
    auto origParameters = _SimulationFacade::get()->getOriginalSimulationParameters();
    auto lastParameters = parameters;

    auto sourceIndex = LocationHelper::findLocationArrayIndex(parameters, _orderNumber);

    _sourceName = std::string(parameters.sourceName.sourceValues[sourceIndex]);

    ImGui::PushID("Source");
    SpecificationGuiService::get().createWidgetsForParameters(parameters, origParameters, _orderNumber, filter);
    ImGui::PopID();

    if (parameters != lastParameters) {
        ParametersValidationService::get().validateAndCorrect({_SimulationFacade::get()->getWorldSize()}, parameters);
        auto isRunning = _SimulationFacade::get()->isSimulationRunning();
        _SimulationFacade::get()->setSimulationParameters(
            parameters, isRunning ? SimulationParametersUpdateConfig::AllExceptChangingPositions : SimulationParametersUpdateConfig::All);
    }
}

std::string _SimulationParametersSourceWidgets::getLocationName()
{
    // 'Source' 설정창의 이름을 한글로 변경합니다.
    return "'" + _sourceName + "' 광원의 시뮬레이션 설정";
}

int _SimulationParametersSourceWidgets::getOrderNumber() const
{
    return _orderNumber;
}

void _SimulationParametersSourceWidgets::setOrderNumber(int orderNumber)
{
    _orderNumber = orderNumber;
}
