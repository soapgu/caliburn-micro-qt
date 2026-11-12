#include <CaliburnMicroQt/IInstanceProvider.h>
#include <QStringList>

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

void InstanceDetail::validate(std::type_index actual, bool present, const InstanceRequest &request,
                              InstanceOperation operation)
{
    if (actual != request.type)
        throw InstanceError(InstanceErrorCode::TypeMismatch, operation, request, QStringLiteral("返回契约类型不匹配"));
    if (!present)
        throw InstanceError(InstanceErrorCode::CreationFailed, operation, request, QStringLiteral("已登记对象为空"));
}

UniqueInstance IInstanceProvider::createUnique(const InstanceRequest &request)
{
    throw InstanceError(InstanceErrorCode::UnsupportedCapability, InstanceOperation::Create, request,
                        QStringLiteral("适配器不支持独占创建"));
}
std::vector<SharedInstance> IInstanceProvider::resolveCollection(const InstanceRequest &request)
{
    throw InstanceError(InstanceErrorCode::UnsupportedCapability, InstanceOperation::ResolveAll, request,
                        QStringLiteral("适配器不支持集合解析"));
}
void IInstanceProvider::injectExisting(const InstanceRequest &request, BorrowedInstance)
{
    throw InstanceError(InstanceErrorCode::UnsupportedCapability, InstanceOperation::BuildUp, request,
                        QStringLiteral("适配器不支持已有对象注入"));
}
