#include "lukspassworddialog.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFont>
#include <QFrame>

LUKSPasswordDialog::LUKSPasswordDialog(QWidget *parent)
    : QDialog(parent),
      m_infoLabel(nullptr),
      m_passwordEdit(nullptr),
      m_showPwdBtn(nullptr),
      m_decryptBtn(nullptr),
      m_cancelBtn(nullptr)
{
    setupUi();
    applyTheme();
}

void LUKSPasswordDialog::setupUi()
{
    setWindowTitle(tr("LUKS Encrypted Volume"));
    setModal(true);
    setMinimumSize(640, 480);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 24, 24, 24);
    mainLayout->setSpacing(16);

    QLabel *iconLabel = new QLabel(QString::fromUtf8("\xF0\x9F\x94\x92"), this);
    QFont iconFont;
    iconFont.setPointSize(32);
    iconLabel->setFont(iconFont);
    iconLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(iconLabel);

    m_infoLabel = new QLabel(this);
    m_infoLabel->setText(
        tr("This partition is encrypted with LUKS.\n"
           "Please enter the passphrase to decrypt and access the data.\n\n"
           "Without the password, you can still use Carve mode to recover\n"
           "files from raw cluster data — this may find files that existed\n"
           "on the disk before encryption was applied."));
    m_infoLabel->setAlignment(Qt::AlignCenter);
    m_infoLabel->setWordWrap(true);
    mainLayout->addWidget(m_infoLabel);

    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText(tr("Enter LUKS passphrase..."));
    m_passwordEdit->setMinimumHeight(36);

    m_showPwdBtn = new QPushButton(tr("Show"), this);
    m_showPwdBtn->setCheckable(true);
    m_showPwdBtn->setMinimumHeight(36);
    m_showPwdBtn->setFixedWidth(64);

    QHBoxLayout *pwdLayout = new QHBoxLayout();
    pwdLayout->setSpacing(8);
    pwdLayout->addWidget(m_passwordEdit);
    pwdLayout->addWidget(m_showPwdBtn);
    mainLayout->addLayout(pwdLayout);

    m_decryptBtn = new QPushButton(tr("Decrypt"), this);
    m_decryptBtn->setDefault(true);
    m_decryptBtn->setMinimumHeight(36);
    m_cancelBtn = new QPushButton(tr("Cancel"), this);
    m_cancelBtn->setMinimumHeight(36);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(16);
    btnLayout->addStretch();
    btnLayout->addWidget(m_decryptBtn);
    btnLayout->addWidget(m_cancelBtn);
    btnLayout->addStretch();
    mainLayout->addLayout(btnLayout);

    mainLayout->addStretch();

    connect(m_decryptBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_passwordEdit, &QLineEdit::returnPressed, m_decryptBtn, &QPushButton::click);
    connect(m_showPwdBtn, &QPushButton::toggled, this, [this](bool checked) {
        m_passwordEdit->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
    });
}

void LUKSPasswordDialog::applyTheme()
{
    setStyleSheet(QString(
        "LUKSPasswordDialog {"
        "  background-color: #2E3440;"
        "  border: 2px solid #B48EAD;"
        "}"
        "QLabel { color: #ECEFF4; }"
        "QLineEdit {"
        "  background-color: #3B4252;"
        "  color: #ECEFF4;"
        "  border: 1px solid #B48EAD;"
        "  padding: 8px 12px;"
        "  selection-background-color: #B48EAD;"
        "}"
        "QPushButton {"
        "  background-color: #434C5E;"
        "  color: #ECEFF4;"
        "  border: 1px solid #4C566A;"
        "  padding: 8px 24px;"
        "  min-width: 120px;"
        "}"
        "QPushButton:hover { background-color: #4C566A; }"
        "QPushButton:default {"
        "  background-color: #B48EAD;"
        "  color: #2E3440;"
        "  font-weight: bold;"
        "  border: 1px solid #B48EAD;"
        "}"
        "QPushButton:default:hover { background-color: #A17BA0; }"
        "QPushButton:checked {"
        "  background-color: #B48EAD;"
        "  color: #2E3440;"
        "}"
    ));
}

QString LUKSPasswordDialog::password() const
{
    return m_passwordEdit->text();
}
