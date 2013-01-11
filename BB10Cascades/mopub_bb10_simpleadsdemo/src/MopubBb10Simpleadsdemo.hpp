#ifndef MopubBb10Simpleadsdemo_HPP_
#define MopubBb10Simpleadsdemo_HPP_

#include <QObject>

namespace bb { namespace cascades { class Application; }}

/*!
 * @brief Application pane object
 *
 *Use this object to create and init app UI, to create context objects, to register the new meta types etc.
 */
class MopubBb10Simpleadsdemo : public QObject
{
    Q_OBJECT
public:
    MopubBb10Simpleadsdemo(bb::cascades::Application *app);
    virtual ~MopubBb10Simpleadsdemo() {}
};


#endif /* MopubBb10Simpleadsdemo_HPP_ */
