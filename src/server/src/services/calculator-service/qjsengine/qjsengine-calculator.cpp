#include "services/calculator-service/qjsengine/qjsengine-calculator.hpp"
#include <QJSEngine>
#include <QJSValue>
#include <QRegularExpression>
#include <QtConcurrent/QtConcurrent>
#include <algorithm>
#include <cctype>
#include <string_view>

using CalculatorResult = QJSEngineCalculator::CalculatorResult;
using CalculatorError = QJSEngineCalculator::CalculatorError;

QJSEngineCalculator::QJSEngineCalculator() = default;

QString QJSEngineCalculator::id() const { return "qjsengine"; }

QString QJSEngineCalculator::displayName() const { return "QJSEngine Calculator"; }

bool QJSEngineCalculator::isActivatable() const { return true; }

bool QJSEngineCalculator::start() {
  if (!m_engine) {
    m_engine = std::make_unique<QJSEngine>();
    m_engine->installExtensions(QJSEngine::ConsoleExtension);
  }
  return true;
}

void QJSEngineCalculator::stop() { m_engine.reset(); }

QString QJSEngineCalculator::preprocessExpression(const QString &input) {
  QString expr = input.trimmed();

  expr.replace(QChar(0x00D7), '*'); // × → *
  expr.replace(QChar(0x00F7), '/'); // ÷ → /
  expr.replace('^', "**");          // ^ → **

  expr.replace(QRegularExpression(R"((\d+(?:\.\d+)?)%)"), "($1/100)");

  expr.replace(QRegularExpression(R"(\bsqrt\()"), "Math.sqrt(");
  expr.replace(QRegularExpression(R"(\bsin\()"), "Math.sin(");
  expr.replace(QRegularExpression(R"(\bcos\()"), "Math.cos(");
  expr.replace(QRegularExpression(R"(\btan\()"), "Math.tan(");
  expr.replace(QRegularExpression(R"(\basin\()"), "Math.asin(");
  expr.replace(QRegularExpression(R"(\bacos\()"), "Math.acos(");
  expr.replace(QRegularExpression(R"(\batan\()"), "Math.atan(");
  expr.replace(QRegularExpression(R"(\blog\()"), "Math.log10(");
  expr.replace(QRegularExpression(R"(\bln\()"), "Math.log(");
  expr.replace(QRegularExpression(R"(\blog2\()"), "Math.log2(");
  expr.replace(QRegularExpression(R"(\babs\()"), "Math.abs(");
  expr.replace(QRegularExpression(R"(\bceil\()"), "Math.ceil(");
  expr.replace(QRegularExpression(R"(\bfloor\()"), "Math.floor(");
  expr.replace(QRegularExpression(R"(\bround\()"), "Math.round(");
  expr.replace(QRegularExpression(R"(\bpi\b)"), "Math.PI");
  expr.replace(QRegularExpression(R"((?<![a-zA-Z])e(?![a-zA-Z]))"), "Math.E");

  expr.replace(QRegularExpression(R"(\bmin\()"), "Math.min(");
  expr.replace(QRegularExpression(R"(\bmax\()"), "Math.max(");
  expr.replace(QRegularExpression(R"(\bpow\()"), "Math.pow(");

  return expr;
}

bool QJSEngineCalculator::isExpression(const std::string &query) const {
  if (query.empty()) return false;

  bool hasDigit = std::ranges::any_of(query, [](unsigned char c) { return std::isdigit(c); });
  if (!hasDigit) return false;

  static constexpr std::string_view OPS = "+-*/^%";
  for (char c : query) {
    if (OPS.find(c) != std::string_view::npos) return true;
  }

  return false;
}

CalculatorResult QJSEngineCalculator::compute(const QString &question, const ComputeOptions &opts) {
  const auto fail = [](auto &&reason) { return std::unexpected(CalculatorError(reason)); };

  if (!m_engine) return fail("Engine not started");

  if (opts.mode == ComputeMode::MixedSearch) {
    if (!isExpression(question.toStdString())) { return fail("Not a valid expression"); }
  }

  QString expression = preprocessExpression(question);
  if (expression.isEmpty()) return fail("Empty expression");

  QJSValue result = m_engine->evaluate(expression);
  if (result.isError()) { return fail(result.toString()); }

  if (!result.isNumber()) { return fail("Result is not a number"); }

  double value = result.toNumber();

  CalculatorResult calcRes;
  calcRes.type = CalculatorAnswerType::NORMAL;
  calcRes.question.text = question;
  calcRes.answer.text = QString::number(value, 'g', 15);

  return calcRes;
}

QFuture<QJSEngineCalculator::ComputeResult> QJSEngineCalculator::asyncCompute(const QString &question,
                                                                              const ComputeOptions &opts) {
  return QtConcurrent::run([this, question, opts]() -> ComputeResult { return compute(question, opts); });
}
