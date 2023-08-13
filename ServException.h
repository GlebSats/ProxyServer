#ifndef SERVEXCEPTION_H
#define SERVEXCEPTION_H

class ServException {
public:
	ServException(char const* error_type, int error_code = 0) : err_type(error_type), err_code(error_code) {

	}
	const char* GetErrorType() const {
		return err_type;
	}
	int GetErrorCode() const {
		return err_code;
	}
private:
	const char* err_type;
	int err_code;
};

#endif
