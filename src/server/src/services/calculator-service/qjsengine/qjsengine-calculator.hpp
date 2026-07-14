#pragma once
#include "services/calculator-service/abstract-calculator-backend.hpp"
#include <QJSEngine>
#include <QFuture>
#include <memory>

class QJSEngineCalculator : public AbstractCalculatorBackend {
public:
  QJSEngineCalculator();

  QString id() const override;
  QString displayName() const override;
  bool isActivatable() const override;
  bool start() override;
  void stop() override;
  ComputeResult compute(const QString &question, const ComputeOptions &opts) override;
  QFuture<ComputeResult> asyncCompute(const QString &question, const ComputeOptions &opts) override;
  bool isExpression(const std::string &query) const override;

private:
  static QString preprocessExpression(const QString &input);

  std::unique_ptr<QJSEngine> m_engine;
};
