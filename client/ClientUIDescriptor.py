from abc import ABC, abstractmethod

class ClientUIDescriptor(ABC):
    '''
    The DebuggerBootstrapClient will poll various file descriptors (fd's), 
    this class provides the user input fd, and the callback for the corresponding fd interactions
    '''
    @abstractmethod
    def get_ui_input_fd(self):
        '''Get an fd that will be polled in order to know if the UI should be updated in some way
        
        See https://docs.python.org/3/library/select.html#select.select for the specification of the object that should be returned.
        Basically it should return either an int representing a "waitable object", or an object with a "fileno()" function returning such an int.
        '''
        pass

    @abstractmethod
    def ui_read_poll_iteration(self):
        '''Will be called by the DebuggerBootstrapClient when the fd obtained from "get_ui_input_fd" is ready to be read from'''
        pass
