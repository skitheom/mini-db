NAME     := mini_db
CXX      := c++            # or clang++
CXXFLAGS := -std=c++17 -Wall -Wextra -Werror -O0 -g -MMD -MP
SRCDIR   := srcs
OBJDIR   := objs
INCLUDES := -I./includes
RM       := rm -rf

SRCS := $(wildcard $(SRCDIR)/*.cpp)
OBJS := $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(OBJS)

-include $(DEPS)

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	$(RM) $(OBJDIR)

fclean: clean
	$(RM) $(NAME)

re: fclean all

run: $(NAME)
	./$(NAME) 1111 .save

run-bg: $(NAME)
	./$(NAME) 1111 .save > .server.out 2>&1 & echo $$! > .server.pid

stop:
	@if [ -f .server.pid ]; then kill -INT $$(cat .server.pid) 2>/dev/null || true; rm -f .server.pid; fi

.PHONY: all clean fclean re run run-bg stop
