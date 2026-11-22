/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "intro/intro_bot_token.h"

#include "config.h"
#include "lang/lang_keys.h"
#include "ui/widgets/fields/input_field.h"
#include "main/main_account.h"
#include "mtproto/mtp_instance.h"
#include "styles/style_intro.h"

namespace Intro {
namespace details {

BotTokenWidget::BotTokenWidget(
	QWidget *parent,
	not_null<Main::Account*> account,
	not_null<Data*> data)
: Step(parent, account, data)
, _token(this, st::introPassword, rpl::single(u"Bot token"_q)) {
	_token->setAccessibleName(u"Bot token"_q);
	_token->setInputMethodHints(Qt::ImhNoAutoUppercase
		| Qt::ImhNoPredictiveText);

	setTitleText(rpl::single(u"Login with Bot Token"_q));
	setDescriptionText(rpl::single(
		u"Enter the bot token you received from @BotFather."_q));
	setErrorCentered(true);

	_token->changes(
	) | rpl::on_next([=] {
		hideError();
	}, _token->lifetime());
	_token->submits(
	) | rpl::on_next([=] {
		submit();
	}, _token->lifetime());
}

void BotTokenWidget::resizeEvent(QResizeEvent *e) {
	Step::resizeEvent(e);
	_token->moveToLeft(contentLeft(), contentTop() + st::introStepFieldTop);
}

void BotTokenWidget::showTokenError(rpl::producer<QString> text) {
	_token->showError();
	showError(std::move(text));
}

void BotTokenWidget::setInnerFocus() {
	_token->setFocusFast();
}

void BotTokenWidget::activate() {
	Step::activate();
	showChildren();
	setInnerFocus();
}

void BotTokenWidget::finished() {
	Step::finished();
	apiClear();

	cancelled();
}

void BotTokenWidget::cancelled() {
	api().request(base::take(_sentRequest)).cancel();
}

void BotTokenWidget::submit() {
	if (_sentRequest || isHidden()) {
		return;
	}
	const auto token = _token->getLastText().trimmed();
	if (token.isEmpty()) {
		showTokenError(rpl::single(u"Please enter a bot token."_q));
		_token->setFocus();
		return;
	}

	hideError();
	cancelNearestDcRequest();

	_token->setEnabled(false);
	_sentRequest = api().request(MTPauth_ImportBotAuthorization(
		MTP_int(0),
		MTP_int(ApiId),
		MTP_string(ApiHash),
		MTP_string(token)
	)).done([=](const MTPauth_Authorization &result) {
		submitDone(result);
	}).fail([=](const MTP::Error &error) {
		submitFail(error);
	}).handleFloodErrors().send();
}

void BotTokenWidget::submitDone(const MTPauth_Authorization &result) {
	_sentRequest = 0;
	_token->setEnabled(true);
	finish(result);
}

void BotTokenWidget::submitFail(const MTP::Error &error) {
	if (MTP::IsFloodError(error)) {
		_sentRequest = 0;
		_token->setEnabled(true);
		_token->setFocus();
		showTokenError(tr::lng_flood_error());
		return;
	}

	_sentRequest = 0;
	_token->setEnabled(true);
	_token->setFocus();
	const auto &err = error.type();
	if (err == u"ACCESS_TOKEN_INVALID"_q) {
		showTokenError(rpl::single(u"Invalid bot token."_q));
	} else if (!MTP::IgnoreError(error)) {
		showTokenError(rpl::single(err));
	}
}

} // namespace details
} // namespace Intro
