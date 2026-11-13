#include <CaliburnMicroQt/IInstanceProvider.h>
#include <QStringList>
#include <utility>

namespace {
QString describe(const InstanceRequest &request)
{
    return QStringLiteral("%1 [name=\"%2\"]")
            .arg(QString::fromUtf8(request.type.name()), request.name);
}
QString message(InstanceErrorCode code, InstanceOperation operation, const InstanceRequest &request,
                const QString &reason, const std::vector<InstanceRequest> &chain)
{
    QStringList steps;
    for (const auto &step : chain) steps << describe(step);
    return QStringLiteral("实例解析失败 code=%1 operation=%2: %3: %4; chain=%5")
            .arg(int(code)).arg(int(operation)).arg(describe(request), reason, steps.join(" -> "));
}
}

InstanceError::InstanceError(InstanceErrorCode code, InstanceOperation operation, InstanceRequest request,
                             QString reason, std::vector<InstanceRequest> dependencyChain)
    : std::runtime_error(message(code, operation, request, reason, dependencyChain).toStdString()),
      m_code(code), m_operation(operation), m_request(std::move(request)), m_chain(std::move(dependencyChain)) {}
