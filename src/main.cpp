#include <ctime>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>


class ICommand {
public:
    virtual void execute(std::ostream&) = 0;
    virtual ~ICommand() = default;
};

class Command : public ICommand {
public:
    virtual ~Command() = default;
    Command(const std::string& cmd): m_cmd(cmd)
    {
    }
    void execute(std::ostream& s) override
    {
        s << m_cmd;
    }
protected:
    const std::string m_cmd;
};

class BulkCommand : public ICommand {
public:
    virtual ~BulkCommand() = default;
    BulkCommand() = default;
    void addCommand(std::unique_ptr<ICommand> cmd) 
    {
        m_commands.emplace_back(std::move(cmd));
    }

    void execute(std::ostream& s) override
    {
        auto it = m_commands.begin();
        if (it != m_commands.end())
        {
            (*it)->execute(s);

            for (++it; it != m_commands.end(); ++it)
            {
                s << ", ";
                (*it)->execute(s);
            }
        }
    }

    size_t size() const
    {
        return m_commands.size();
    }

    void clear()
    {
        m_commands.clear();
    }
protected:
    std::vector<std::unique_ptr<ICommand>> m_commands;
};

class BulkCommandOperator;

class IBulkCommandHandler
{
public:
    virtual ~IBulkCommandHandler() {};
    virtual void startBulk(BulkCommandOperator*) = 0;
    virtual void endBulk(BulkCommandOperator*) = 0;
    virtual void addCommand(const std::string& cmd) = 0;
};

class BulkCommandHandler: public IBulkCommandHandler
{
public:
    virtual void addCommand(const std::string& cmd) override
    {
        if (bulkCommand.size() == 0)
            timestamp = std::time(nullptr);

        bulkCommand.addCommand(std::unique_ptr<ICommand>(new Command(cmd)));

    }

protected:
    void processBulk()
    {
        if (bulkCommand.size() != 0)
        {
            std::cout << "bulk: ";
            bulkCommand.execute(std::cout);
            std::cout << std::endl;

            std::ofstream f(std::string("bulk") + std::to_string(timestamp) + ".log", std::ios_base::ate | std::ios_base::app);
            if (f.tellp() != 0)
                f << std::endl;

            bulkCommand.execute(f);

            bulkCommand.clear();
        }
    }

    BulkCommand bulkCommand;
    time_t timestamp;
};

class FixedBulkCommandHandler : public BulkCommandHandler
{
public:
    FixedBulkCommandHandler(int bulkN) : m_bulkN(bulkN) { }
    virtual ~FixedBulkCommandHandler()
    {
        processBulk();
    }

    virtual void startBulk(BulkCommandOperator* bulkCmdOp) override;
    virtual void endBulk(BulkCommandOperator* bulkCmdOp) override { }

    virtual void addCommand(const std::string& cmd) override
    {
        BulkCommandHandler::addCommand(cmd);
        if (bulkCommand.size() == m_bulkN)
            processBulk();
    };
private:
    int m_bulkN;
};

class DynamicBulkCommandHandler : public BulkCommandHandler
{
public:
    virtual ~DynamicBulkCommandHandler()
    {
        if(nested_bulk==0)
            processBulk();
    }

    virtual void startBulk(BulkCommandOperator* bulkCmdOp) override
    {
        ++nested_bulk;
    }
    virtual void endBulk(BulkCommandOperator* bulkCmdOp) override;
private:
    int nested_bulk = 1;
};

class BulkCommandOperator
{
public:
    BulkCommandOperator(int bulkN) : m_bulkN(bulkN)
    {
        setHandler(std::unique_ptr<IBulkCommandHandler>(new FixedBulkCommandHandler(bulkN)));
    }
    void setHandler(std::unique_ptr<IBulkCommandHandler> handler)
    {
        m_handler = std::move(handler);
    }
    void startBulk()
    {
        m_handler->startBulk(this);
    }
    void endBulk()
    {
        m_handler->endBulk(this);
    }
    void addCommand(const std::string& cmd)
    {
        m_handler->addCommand(cmd);
    }
    int bulkN(void)
    {
        return m_bulkN;
    }

private:

    int m_bulkN = 0;
    std::unique_ptr<IBulkCommandHandler> m_handler;
};

void FixedBulkCommandHandler::startBulk(BulkCommandOperator* bulkCmdOp)
{
    bulkCmdOp->setHandler(std::unique_ptr<IBulkCommandHandler>(new DynamicBulkCommandHandler));
    //do not write code here
}

void DynamicBulkCommandHandler::endBulk(BulkCommandOperator* bulkCmdOp)
{
    if (--nested_bulk == 0)
        bulkCmdOp->setHandler(std::unique_ptr<IBulkCommandHandler>(new FixedBulkCommandHandler(bulkCmdOp->bulkN())));
        //do not write code here
}


int main(int argc, char const *argv[])
{
    try
    {
        int bulkN = 0;
        if (argc == 2)
            bulkN = std::atoi(argv[1]);
        else
            throw std::exception("./bulk <N>");

        BulkCommandOperator bulkCommandOperator(bulkN);

        for(std::string line; std::getline(std::cin, line);)
        {            
            if (line == "{")
                bulkCommandOperator.startBulk();
            else if (line == "}")
                bulkCommandOperator.endBulk();
            else
                bulkCommandOperator.addCommand(line);
        }
    }
    catch(const std::exception &e)
    {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}
