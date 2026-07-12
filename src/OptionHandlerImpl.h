/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2006 Tatsuhiro Tsujikawa
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
 */
/* copyright --> */
#ifndef D_OPTION_HANDLER_IMPL_H
#define D_OPTION_HANDLER_IMPL_H

#include "OptionHandler.h"

#include <vector>

#include "AbstractOptionHandler.h"

namespace aria2 {

class Option;
struct Pref;

class BooleanOptionHandler : public AbstractOptionHandler {
public:
  BooleanOptionHandler(PrefPtr pref, const char* description = NO_DESCRIPTION,
                       const std::string& defaultValue = NO_DEFAULT_VALUE,
                       OptionHandler::ARG_TYPE argType = OptionHandler::REQ_ARG,
                       char shortName = 0);
  virtual ~BooleanOptionHandler();
  virtual void parseArg(Option& option,
                        const std::string& optarg) const override;
  virtual std::string createPossibleValuesString() const override;
};

class IntegerRangeOptionHandler : public AbstractOptionHandler {
private:
  int32_t min_;
  int32_t max_;

public:
  IntegerRangeOptionHandler(PrefPtr pref, const char* description,
                            const std::string& defaultValue, int32_t min,
                            int32_t max, char shortName = 0);
  virtual ~IntegerRangeOptionHandler();
  virtual void parseArg(Option& option,
                        const std::string& optarg) const override;
  virtual std::string createPossibleValuesString() const override;
};

class NumberOptionHandler : public AbstractOptionHandler {
private:
  int64_t min_;
  int64_t max_;

public:
  NumberOptionHandler(PrefPtr pref, const char* description = NO_DESCRIPTION,
                      const std::string& defaultValue = NO_DEFAULT_VALUE,
                      int64_t min = -1, int64_t max = -1, char shortName = 0);
  virtual ~NumberOptionHandler();

  virtual void parseArg(Option& option,
                        const std::string& optarg) const override;
  void parseArg(Option& option, int64_t number) const;
  virtual std::string createPossibleValuesString() const override;
};

class UnitNumberOptionHandler : public NumberOptionHandler {
public:
  UnitNumberOptionHandler(PrefPtr pref,
                          const char* description = NO_DESCRIPTION,
                          const std::string& defaultValue = NO_DEFAULT_VALUE,
                          int64_t min = -1, int64_t max = -1,
                          char shortName = 0);
  virtual ~UnitNumberOptionHandler();
  virtual void parseArg(Option& option,
                        const std::string& optarg) const override;
};

class FloatNumberOptionHandler : public AbstractOptionHandler {
private:
  double min_;
  double max_;

public:
  FloatNumberOptionHandler(PrefPtr pref,
                           const char* description = NO_DESCRIPTION,
                           const std::string& defaultValue = NO_DEFAULT_VALUE,
                           double min = -1, double max = -1,
                           char shortName = 0);
  virtual ~FloatNumberOptionHandler();
  virtual void parseArg(Option& option,
                        const std::string& optarg) const override;
  virtual std::string createPossibleValuesString() const override;
};

class DefaultOptionHandler : public AbstractOptionHandler {
private:
  std::string possibleValuesString_;
  bool allowEmpty_;

public:
  DefaultOptionHandler(PrefPtr pref, const char* description = NO_DESCRIPTION,
                       const std::string& defaultValue = NO_DEFAULT_VALUE,
                       const std::string& possibleValuesString = "",
                       OptionHandler::ARG_TYPE argType = OptionHandler::REQ_ARG,
                       char shortName = 0);
  virtual ~DefaultOptionHandler();
  virtual void parseArg(Option& option,
                        const std::string& optarg) const override;
  virtual std::string createPossibleValuesString() const override;
  void setAllowEmpty(bool allow);
};

class CumulativeOptionHandler : public AbstractOptionHandler {
private:
  std::string delim_;
  std::string possibleValuesString_;

public:
  CumulativeOptionHandler(
      PrefPtr pref, const char* description, const std::string& defaultValue,
      const std::string& delim,
      const std::string& possibleValuesString = "",
      OptionHandler::ARG_TYPE argType = OptionHandler::REQ_ARG,
      char shortName = 0);
  virtual ~CumulativeOptionHandler();
  virtual void parseArg(Option& option,
                        const std::string& optarg) const override;
  virtual std::string createPossibleValuesString() const override;
};

class IndexOutOptionHandler : public AbstractOptionHandler {
public:
  IndexOutOptionHandler(PrefPtr pref, const char* description,
                        char shortName = 0);
  virtual ~IndexOutOptionHandler();
  virtual void parseArg(Option& option,
                        const std::string& optarg) const override;
  virtual std::string createPossibleValuesString() const override;
};

class ChecksumOptionHandler : public AbstractOptionHandler {
public:
  ChecksumOptionHandler(PrefPtr pref, const char* description,
                        char shortName = 0);
  ChecksumOptionHandler(PrefPtr pref, const char* description,
                        std::vector<std::string> acceptableTypes,
                        char shortName = 0);
  virtual ~ChecksumOptionHandler();
  virtual void parseArg(Option& option,
                        const std::string& optarg) const override;
  virtual std::string createPossibleValuesString() const override;

private:
  // message digest type acceptable for this option.  Empty means that
  // it accepts all supported types.
  std::vector<std::string> acceptableTypes_;
};

class ParameterOptionHandler : public AbstractOptionHandler {
private:
  std::vector<std::string> validParamValues_;

public:
  ParameterOptionHandler(PrefPtr pref, const char* description,
                         const std::string& defaultValue,
                         std::vector<std::string> validParamValues,
                         char shortName = 0);
  virtual ~ParameterOptionHandler();
  virtual void parseArg(Option& option,
                        const std::string& optarg) const override;
  virtual std::string createPossibleValuesString() const override;
};

class HostPortOptionHandler : public AbstractOptionHandler {
private:
  PrefPtr hostOptionName_;
  PrefPtr portOptionName_;

public:
  HostPortOptionHandler(PrefPtr pref, const char* description,
                        const std::string& defaultValue, PrefPtr hostOptionName,
                        PrefPtr portOptionName, char shortName = 0);
  virtual ~HostPortOptionHandler();
  virtual void parseArg(Option& option,
                        const std::string& optarg) const override;
  void setHostAndPort(Option& option, const std::string& hostname,
                      uint16_t port) const;
  virtual std::string createPossibleValuesString() const override;
};

class HttpProxyOptionHandler : public AbstractOptionHandler {
private:
  PrefPtr proxyUserPref_;
  PrefPtr proxyPasswdPref_;

public:
  HttpProxyOptionHandler(PrefPtr pref, const char* description,
                         const std::string& defaultValue, char shortName = 0);
  virtual ~HttpProxyOptionHandler();
  virtual void parseArg(Option& option,
                        const std::string& optarg) const override;
  virtual std::string createPossibleValuesString() const override;
};

class LocalFilePathOptionHandler : public AbstractOptionHandler {
private:
  std::string possibleValuesString_;
  bool acceptStdin_;
  bool mustExist_;

public:
  LocalFilePathOptionHandler(PrefPtr pref,
                             const char* description = NO_DESCRIPTION,
                             const std::string& defaultValue = NO_DEFAULT_VALUE,
                             bool acceptStdin = false, char shortName = 0,
                             bool mustExist = true,
                             const std::string& possibleValuesString = "");
  virtual void parseArg(Option& option,
                        const std::string& optarg) const override;
  virtual std::string createPossibleValuesString() const override;
};

class PrioritizePieceOptionHandler : public AbstractOptionHandler {
public:
  PrioritizePieceOptionHandler(
      PrefPtr pref, const char* description = NO_DESCRIPTION,
      const std::string& defaultValue = NO_DEFAULT_VALUE, char shortName = 0);
  virtual void parseArg(Option& option,
                        const std::string& optarg) const override;
  virtual std::string createPossibleValuesString() const override;
};

class OptimizeConcurrentDownloadsOptionHandler : public AbstractOptionHandler {
public:
  OptimizeConcurrentDownloadsOptionHandler(
      PrefPtr pref, const char* description = NO_DESCRIPTION,
      const std::string& defaultValue = NO_DEFAULT_VALUE, char shortName = 0);
  virtual void parseArg(Option& option,
                        const std::string& optarg) const override;
  virtual std::string createPossibleValuesString() const override;
};

// This class is used to deprecate option and optionally handle its
// option value using replacing option.
class DeprecatedOptionHandler : public OptionHandler {
private:
  OptionHandler* depOptHandler_;
  const OptionHandler* repOptHandler_;
  bool stillWork_;
  std::string additionalMessage_;

public:
  // depOptHandler is deprecated option and repOptHandler is replacing
  // new option. If there is no replacing option, specify nullptr.  If
  // there is no replacing option, but the option still lives, give
  // true to stillWork. Set additional message to additionalMessage.
  DeprecatedOptionHandler(OptionHandler* depOptHandler,
                          const OptionHandler* repOptHandler = nullptr,
                          bool stillWork = false,
                          std::string additionalMessage = "");
  virtual ~DeprecatedOptionHandler();
  virtual void parse(Option& option,
                     const std::string& arg) const override;
  virtual std::string createPossibleValuesString() const override;
  virtual bool hasTag(uint32_t tag) const override;
  virtual void addTag(uint32_t tag) override;
  virtual std::string toTagString() const override;
  virtual const char* getName() const override;
  virtual const char* getDescription() const override;
  virtual const std::string& getDefaultValue() const override;
  virtual bool isHidden() const override;
  virtual void hide() override;
  virtual PrefPtr getPref() const override;
  virtual ARG_TYPE getArgType() const override;
  virtual char getShortName() const override;
  virtual bool getEraseAfterParse() const override;
  virtual void setEraseAfterParse(bool eraseAfterParse) override;
  virtual bool getInitialOption() const override;
  virtual void setInitialOption(bool f) override;
  virtual bool getChangeOption() const override;
  virtual void setChangeOption(bool f) override;
  virtual bool getChangeOptionForReserved() const override;
  virtual void setChangeOptionForReserved(bool f) override;
  virtual bool getChangeGlobalOption() const override;
  virtual void setChangeGlobalOption(bool f) override;
  virtual bool getCumulative() const override;
  virtual void setCumulative(bool f) override;
};

} // namespace aria2

#endif // D_OPTION_HANDLER_IMPL_H
