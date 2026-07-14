#pragma once
#include "services/calculator-service/abstract-calculator-backend.hpp"
#include <qdebug.h>

class DummyCalculatorBackend : public AbstractCalculatorBackend {
public:
  QString id() const override { return "dummy"; }
  QString displayName() const override { return "No backend available"; }

  bool isActivatable() const override { return true; }

  bool start() override {
    qWarning() << "Started dummy calculator backend! No real calculator backend is available on this platform. "
                  "Calculations will not work. Help contribute calculator support: https://docs.vicinae.com/";
    return true;
  }

  ComputeResult compute(const QString &question) override {
    return std::unexpected(CalculatorError("No calculator backend available"));
  }

  QFuture<ComputeResult> asyncCompute(const QString &question) override {
    QPromise<ComputeResult> promise;
    promise.addResult(std::unexpected(CalculatorError("No calculator backend available")));
    promise.finish();
    return promise.future();
  }
};
