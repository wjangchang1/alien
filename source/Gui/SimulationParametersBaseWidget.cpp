#include "SimulationParametersBaseWidget.h"

#include <imgui.h>

#include <EngineInterface/Description.h>
#include <EngineInterface/ParametersEditService.h>
#include <EngineInterface/ParametersValidationService.h>
#include <EngineInterface/SimulationFacade.h>
#include <EngineInterface/SimulationParametersUpdateConfig.h>

#include "AlienGui.h"
#include "SpecificationGuiService.h"

void _SimulationParametersBaseWidget::init() {}

void _SimulationParametersBaseWidget::process(ParametersFilter const& filter)
{
    auto parameters = _SimulationFacade::get()->getSimulationParameters();
    auto origParameters = _SimulationFacade::get()->getOriginalSimulationParameters();
    auto lastParameters = parameters;

    SpecificationGuiService::get().createWidgetsForParameters(parameters, origParameters, 0, filter);

    if (parameters != lastParameters) {
        ParametersValidationService::get().validateAndCorrect({_SimulationFacade::get()->getWorldSize()}, parameters);
        _SimulationFacade::get()->setSimulationParameters(parameters, SimulationParametersUpdateConfig::AllExceptChangingPositions);
    }
}

std::string _SimulationParametersBaseWidget::getLocationName()
{
    // 'Base' 영역의 이름을 한글로 변경합니다.
    return "기본 구역의 시뮬레이션 설정";
}

int _SimulationParametersBaseWidget::getOrderNumber() const
{
    return 0;
}

void _SimulationParametersBaseWidget::setOrderNumber(int orderNumber)
{
    // do nothing
}
