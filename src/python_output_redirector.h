#ifndef PYTHON_OUTPUT_REDIRECTOR_H
#define PYTHON_OUTPUT_REDIRECTOR_H

#include <string>
#include <QString>
#include <QDebug>

namespace FlySight {

struct PythonOutputRedirector {
    void write(const std::string& message_str) {
        QString message = QString::fromStdString(message_str);
        // Python's print usually includes a newline. qDebug also adds one.
        // To avoid double newlines, we can trim the one from Python if present.
        if (message.endsWith(QLatin1Char('\n'))) {
            message.chop(1);
        }
        // Avoid printing completely empty messages if Python sends them
        if (!message.trimmed().isEmpty()) {
            qDebug().noquote() << "[Python]" << message;
        }
    }

    void flush() {
        // qDebug() is often line-buffered or flushes automatically.
        // This method is required by Python's IO interface but might be a no-op here.
    }
};

} // namespace FlySight

#endif // PYTHON_OUTPUT_REDIRECTOR_H
